#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif


typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // OR
    PREC_AND,        // AND
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth; //< How deep in scope the variable is. 0 is global, 1 is first block, etc.
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

/**
 * @brief Used for determining top-level code vs body of function code
 */
typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount; //< How many locals are in scope.
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth; //< Number of blocks surrounding the current part of code we're compiling.
} Compiler;

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

static Chunk* currentChunk() {
    return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

/**
 * @brief Report error at previous token
 * @param message 
 */
static void error(const char* message) {
    errorAt(&parser.previous, message);
}

/**
 * @brief Report error at current token
 * @param message 
 */
static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

/**
 * @brief Update the parser's previous and current token, breaking if we hit a TOKEN_ERROR
 */
static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if(parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

/**
 * @brief Read the next token, but report error if it doesn't match what's expected. Advances the parser cursor if matched.
 * @param type 
 * @param message 
 */
static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

/**
 * @brief Op code for a loop - sets the offset to jump to back to where it was called,
 * so the code knows where to jump back to to restart a loop.
 * @param loopStart 
 */
static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    // Placeholder operand for jump. Using backpatching, where we don't know how far 
    // to jump until we get further along with compiling. We replace this placeholder 
    // once we know that.
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

/**
 * @brief Function used for leaving a function without a return statement.
 * We return a NIL as the return value, which is "stored" at the top of the stack when done.
 */
static void emitReturn() {
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
}

/**
 * @brief Add a constant to the constant table and make sure we don't have too many
 * @param value constant to add
 * @return the index of where we added the constant
 */
static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

/**
 * @brief Update the placeholder 0xff in the jump target with the bytecode to jump to
 * When you see this, think "have the offset passed in be where I want the jump to end up."
 * @param offset 
 */
static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;

}

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start,
                                             parser.previous.length);
    }

    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL 
            ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    // Get rid of all variables at this scope when we leave it.
    while (current->localCount > 0 &&
           current->locals[current->localCount -1].depth > current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
    }
}

// Need these due to recursion.
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

/**
 * @brief Walk backwards through our compiler variables trying to find the most recent one.
 * If found, we return where it's at in the array, which should mirror the VM's stack at runtime.
 * By going backwards, we enable variable shadowing for newer variables with the same name to match before
 * older ones.
 * @param compiler Global compiler variable to search.
 * @param name Name of the variable to search for.
 * @return Index location of variable if found, otherwise -1.
 */
static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        // Don't allow var a = a;, even if a is declared in an outer scope.
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1; // Assume global variable
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    // If we already have the upvalue, return that early.
    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

/**
 * @brief Look at the enclosing function to see if we can find a variable decalred there
 * @param compiler 
 * @param name 
 * @return 
 */
static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    // If we have a local variable in the enclosing function, capture it.
    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    // Otherwise, capture the upvalue of the enclosing function.
    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1){
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

/**
 * @brief Add a local variable to the compiler's 'locals' array.
 * @param name Name of variable to store.
 */
static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables to function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}

/**
 * @brief Declaring a local variable tells the compiler that a variable exists and track it.
 */
static void declareVariable() {
    if (current->scopeDepth == 0 ) return; // Break out of this for global variables. Local only!

    Token* name = &parser.previous;

    // Don't allow multiple var delcarations for the same variable in the same scope.
    for (int i = current->localCount - 1; i >= 0; i --) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(*name);
}

/**
 * @brief Parse a variable, which is the identifier after the var or fun keyword. Store its name in constants table.
 * We break early here if we're not doing a global lookup.
 * @param errorMessage 
 * @return index of where the global name was added in constant table
 */
static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0 ) return 0;

    return identifierConstant(&parser.previous);
}

/**
 * @brief Mark a variable as initalized, setting its depth to the current scope depth.
 */
static void markInitialized() {
    // If called from a global function, we break early.
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    // Break out if we're declaring a local variable.
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

/**
 * @brief returns number of arguments for a function call
*/
static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

/**
 * @brief AND handler. If left operand (already at top of stack) is false, we skip over the right part.
 * @param canAssign 
 */
static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    // If true, evaluate the right side
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    // If false, end up jumped here and skip over the rest of this function.
    patchJump(endJump);
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:      emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:     emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:         emitByte(OP_GREATER); break;
        // Since we only have EQUAL, LESS, GREATER, we're switching the logic around
        // a >= b  === !(a < b)
        case TOKEN_GREATER_EQUAL:   emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:            emitByte(OP_LESS); break;
        // a <= b === !(a > b)
        case TOKEN_LESS_EQUAL:      emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:            emitByte(OP_ADD); break;
        case TOKEN_MINUS:           emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:            emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:           emitByte(OP_DIVIDE); break;
        default: return; // Unreachable.
    }
}

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
    case TOKEN_FALSE: emitByte(OP_FALSE); break;
    case TOKEN_NIL: emitByte(OP_NIL); break;
    case TOKEN_TRUE: emitByte(OP_TRUE);
    default: return; // Unreachable.
    }
}

/**
 * @brief After consuming a (, recurse into expression until we hit )
 */
static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

// Get a string, with +1 and -2 removing quotes around the token
static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                    parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1 ) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }
    
    // If we detect a = after the variable name, we're setting, not getting.
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    // Compile the operand
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return; // Unreachable.
    }
}

/**
 * @brief Table of token rules to know what prefix and infix functions to call, and what precendence they have
 */
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      =   {grouping,  call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]     =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]      =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE]     =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_COMMA]           =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_DOT]             =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_MINUS]           =   {unary,     binary, PREC_TERM},
    [TOKEN_PLUS]            =   {NULL,      binary, PREC_TERM},
    [TOKEN_SEMICOLON]       =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_SLASH]           =   {NULL,      binary, PREC_FACTOR},
    [TOKEN_STAR]            =   {NULL,      binary, PREC_FACTOR},
    [TOKEN_BANG]            =   {unary,     NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]      =   {NULL,      binary, PREC_EQUALITY},
    [TOKEN_EQUAL]           =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]     =   {NULL,      binary, PREC_EQUALITY},
    [TOKEN_GREATER]         =   {NULL,      binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   =   {NULL,      binary, PREC_COMPARISON},
    [TOKEN_LESS]            =   {NULL,      binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      =   {NULL,      binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]      =   {variable,  NULL,   PREC_NONE},
    [TOKEN_STRING]          =   {string,    NULL,   PREC_NONE},
    [TOKEN_NUMBER]          =   {number,    NULL,   PREC_NONE},
    [TOKEN_AND]             =   {NULL,      and_,   PREC_AND},
    [TOKEN_CLASS]           =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_ELSE]            =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_FALSE]           =   {literal,   NULL,   PREC_NONE},
    [TOKEN_FOR]             =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_FUN]             =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_IF]              =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_NIL]             =   {literal,   NULL,   PREC_NONE},
    [TOKEN_OR]              =   {NULL,      or_,    PREC_OR},
    [TOKEN_PRINT]           =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_RETURN]          =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_SUPER]           =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_THIS]            =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_TRUE]            =   {literal,   NULL,   PREC_NONE},
    [TOKEN_VAR]             =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_WHILE]           =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_ERROR]           =   {NULL,      NULL,   PREC_NONE},
    [TOKEN_EOF]             =   {NULL,      NULL,   PREC_NONE},
};

/**
 * @brief Consume the next token, look up its prefix function and run it
 *        Then recurse into infix operators until we're done.
 * @param precedence 
 */
static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name");

    // Initalizer
    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        // Varables without initalizer are nil by default.
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    defineVariable(global);
}

/**
 * @brief An expression, followed by a semicolon.
 */
static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void forStatement() {
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    // Handle for (;...), for (var x = 1;...), and for (x=1;...)
    if(match(TOKEN_SEMICOLON)) {
        // No initalizer.
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        // This looks for a semicolon and adds a OP_POP, which we need, over statement()
        expressionStatement();
    }

    int loopStart = currentChunk()->count;
    int exitJump = -1;
    // Handle for (...;;...) and for (...; x < 5;...)
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is faslse
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);
    }

    // Handle incrementing. We jump over the increment, run the body, jump back to the increment
    // and run it, then jump to the next iteration. Jumps all the way down.

    if (!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        // Emit a loop back to the start of the condition statement
        emitLoop(loopStart);
        // Update loopStart so after we do the main body of the for loop, its jump
        // will end up at the increment expression.
        loopStart = incrementStart;
        // Jumps over the increment expression for now, the body block will bring us back.
        patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition.
    }
    endScope();
}

/**
 * @brief if statement implementation
 * We consume tokens, and check the if statement.
 * If it's false, we jump to the `patchJump(thenJump)` line,
 * which then does a pop and checks for an else statement.
 * 
 * If it's true, we pop and do a statement, and jump to the
 * `patchJump(elseJump)` line so we don't do the else as well.
 */
static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(); // Run through the if condition
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE); // If false, skip then.
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP); // Need to jump over else if true.

    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    // OP_CLOSURE's first operand is 1 if the variable is local, 0 for an upvalue
    // the second operand is the index of the variable/upvalue.
    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }

}

static void funDeclaration() {
    uint8_t global = parseVariable("Expect function name");
    // We can mark as initalized now unlike with variables, since we can have
    // function calls in the function definition, to make recursion a thing.
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }
    
    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        expression();
        consume(TOKEN_SEMICOLON,"Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void whileStatement() {
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'." );
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            
            default:
                ; // Do nothing.
        }

        advance();
    }
}

static void declaration() {
    if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    // Synchronize if we hit error parsing previous statement.
    if (parser.panicMode) synchronize();

}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

ObjFunction* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }
    
    ObjFunction* function = endCompiler();

    // Return false if error occured
    return parser.hadError ? NULL : function;
}

// conscript.cpp
// Implementation of CONScript parser and runtime for an unnamed EDuke32 fork.
// CONScript is a derivative of classic CON scripting, enhanced with syntax inspired by
// DEHACKED (patch-style modifications), DECORATE (actor definitions with properties/states),
// and ZScript (class-based scripting with functions/events).
// This allows more modular, object-oriented modding while maintaining compatibility with base CON.
// Features:
// - Actor classes with inheritance (like ZScript/DECORATE)
// - Property patches (like DEHACKED)
// - State machines for actors (DECORATE-style)
// - Event handlers and custom functions
// - Backward-compatible with classic CON syntax where possible
// Assumptions:
// - Integrates with EDuke32's existing actor/sprite system (e.g., tsprite, actor_t)
// - Uses a simple tokenizer and recursive descent parser
// - Runtime uses a VM similar to CON's bytecode, but extended for OOP features
// For 2025-2026 era fork development.

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>  // for tolower/toupper

#include "build.h"      // EDuke32 headers (adjust as per fork)
#include "baselayer.h"
#include "cache1d.h"
#include "script.h"     // Assume existing CON script header

// Forward declarations
struct ActorClass;
struct State;
struct Function;
class CONScriptVM;

// Token types for parser
enum TokenType {
    TOK_EOF,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING,
    TOK_LBRACE,     // {
    TOK_RBRACE,     // }
    TOK_LPAREN,     // (
    TOK_RPAREN,     // )
    TOK_COLON,      // :
    TOK_SEMICOLON,  // ;
    TOK_COMMA,      // ,
    TOK_EQUALS,     // =
    TOK_PLUS,       // +
    TOK_MINUS,      // -
    TOK_ASTERISK,   // *
    TOK_SLASH,      // /
    TOK_KEYWORD_ACTOR,
    TOK_KEYWORD_CLASS,
    TOK_KEYWORD_INHERITS,
    TOK_KEYWORD_PROPERTY,
    TOK_KEYWORD_STATE,
    TOK_KEYWORD_FUNCTION,
    TOK_KEYWORD_EVENT,
    TOK_KEYWORD_IF,
    TOK_KEYWORD_ELSE,
    TOK_KEYWORD_WHILE,
    TOK_KEYWORD_RETURN,
    // ... add more as needed (e.g., for CON compat: gamevar, define, etc.)
};

// Token structure
struct Token {
    TokenType type;
    std::string value;
    int line;
};

// Actor property types (DEH-like patches)
enum PropertyType {
    PROP_HEALTH,
    PROP_DAMAGE,
    PROP_SPEED,
    PROP_SOUND,
    PROP_SPRITE,
    // ... extend with Build-specific props (e.g., cstat, shade, lotag)
};

// Actor class definition (ZScript/DECORATE-like)
struct ActorClass {
    std::string name;
    std::string parent;  // inheritance
    std::map<std::string, int> properties;  // key-value props
    std::map<std::string, State*> states;   // state machines
    std::map<std::string, Function*> functions;  // custom funcs
    std::vector<std::string> events;  // event handlers (e.g., OnSpawn, OnDeath)
};

// State definition (DECORATE-style)
struct State {
    std::string name;
    std::vector<std::string> actions;  // script lines or function calls
    int duration;  // ticks
    std::string nextState;
    bool loop;
};

// Function definition (ZScript-like)
struct Function {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> body;  // script lines
};

// Simple tokenizer
class Tokenizer {
public:
    Tokenizer(const std::string& source) : src(source), pos(0), line(1) {}

    Token next() {
        skipWhitespace();
        if (pos >= src.size()) return {TOK_EOF, "", line};

        char c = src[pos];
        if (std::isdigit(c) || (c == '-' && std::isdigit(src[pos+1]))) {
            return parseNumber();
        } else if (std::isalpha(c) || c == '_') {
            return parseIdentifier();
        } else if (c == '"') {
            return parseString();
        } else {
            pos++;
            switch (c) {
                case '{': return {TOK_LBRACE, "{", line};
                case '}': return {TOK_RBRACE, "}", line};
                case '(': return {TOK_LPAREN, "(", line};
                case ')': return {TOK_RPAREN, ")", line};
                case ':': return {TOK_COLON, ":", line};
                case ';': return {TOK_SEMICOLON, ";", line};
                case ',': return {TOK_COMMA, ",", line};
                case '=': return {TOK_EQUALS, "=", line};
                case '+': return {TOK_PLUS, "+", line};
                case '-': return {TOK_MINUS, "-", line};
                case '*': return {TOK_ASTERISK, "*", line};
                case '/': return {TOK_SLASH, "/", line};
                default: 
                    std::cerr << "Unexpected char at line " << line << ": " << c << std::endl;
                    return {TOK_EOF, "", line};
            }
        }
    }

private:
    std::string src;
    size_t pos;
    int line;

    void skipWhitespace() {
        while (pos < src.size() && (std::isspace(src[pos]) || src[pos] == '/')) {
            if (src[pos] == '/') {
                if (src[pos+1] == '/') {  // line comment
                    while (pos < src.size() && src[pos] != '\n') pos++;
                } else if (src[pos+1] == '*') {  // block comment
                    pos += 2;
                    while (pos < src.size() && !(src[pos] == '*' && src[pos+1] == '/')) {
                        if (src[pos] == '\n') line++;
                        pos++;
                    }
                    pos += 2;
                } else {
                    break;
                }
            } else if (src[pos] == '\n') {
                line++;
            }
            pos++;
        }
    }

    Token parseNumber() {
        size_t start = pos;
        if (src[pos] == '-') pos++;
        while (pos < src.size() && std::isdigit(src[pos])) pos++;
        return {TOK_NUMBER, src.substr(start, pos - start), line};
    }

    Token parseIdentifier() {
        size_t start = pos;
        while (pos < src.size() && (std::isalnum(src[pos]) || src[pos] == '_')) pos++;
        std::string id = src.substr(start, pos - start);
        // Check for keywords
        if (id == "actor") return {TOK_KEYWORD_ACTOR, id, line};
        if (id == "class") return {TOK_KEYWORD_CLASS, id, line};
        if (id == "inherits") return {TOK_KEYWORD_INHERITS, id, line};
        if (id == "property") return {TOK_KEYWORD_PROPERTY, id, line};
        if (id == "state") return {TOK_KEYWORD_STATE, id, line};
        if (id == "function") return {TOK_KEYWORD_FUNCTION, id, line};
        if (id == "event") return {TOK_KEYWORD_EVENT, id, line};
        if (id == "if") return {TOK_KEYWORD_IF, id, line};
        if (id == "else") return {TOK_KEYWORD_ELSE, id, line};
        if (id == "while") return {TOK_KEYWORD_WHILE, id, line};
        if (id == "return") return {TOK_KEYWORD_RETURN, id, line};
        return {TOK_IDENTIFIER, id, line};
    }

    Token parseString() {
        pos++;  // skip opening "
        size_t start = pos;
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\n') line++;
            pos++;
        }
        pos++;  // skip closing "
        return {TOK_STRING, src.substr(start, pos - start - 1), line};
    }
};

// Parser class
class Parser {
public:
    Parser(const std::string& source) : tokenizer(source), current(tokenizer.next()) {
        classes.clear();
    }

    bool parse() {
        while (current.type != TOK_EOF) {
            if (current.type == TOK_KEYWORD_CLASS || current.type == TOK_KEYWORD_ACTOR) {
                parseClass();
            } else if (current.type == TOK_KEYWORD_PROPERTY) {
                parsePropertyPatch();
            } else {
                // Handle classic CON statements or error
                std::cerr << "Unexpected token at line " << current.line << ": " << current.value << std::endl;
                advance();
            }
        }
        return true;
    }

    const std::unordered_map<std::string, ActorClass*>& getClasses() const { return classes; }

private:
    Tokenizer tokenizer;
    Token current;
    std::unordered_map<std::string, ActorClass*> classes;

    void advance() { current = tokenizer.next(); }

    void expect(TokenType type) {
        if (current.type == type) {
            advance();
        } else {
            std::cerr << "Expected " << type << " at line " << current.line << std::endl;
        }
    }

    void parseClass() {
        bool isActor = (current.type == TOK_KEYWORD_ACTOR);
        advance();  // consume class/actor
        std::string name = current.value;
        expect(TOK_IDENTIFIER);

        std::string parent;
        if (current.type == TOK_KEYWORD_INHERITS) {
            advance();
            parent = current.value;
            expect(TOK_IDENTIFIER);
        }

        ActorClass* cls = new ActorClass{name, parent};

        expect(TOK_LBRACE);

        while (current.type != TOK_RBRACE && current.type != TOK_EOF) {
            if (current.type == TOK_KEYWORD_PROPERTY) {
                parseProperty(cls);
            } else if (current.type == TOK_KEYWORD_STATE) {
                parseState(cls);
            } else if (current.type == TOK_KEYWORD_FUNCTION) {
                parseFunction(cls);
            } else if (current.type == TOK_KEYWORD_EVENT) {
                parseEvent(cls);
            } else {
                // Parse body statements (CON-like)
                parseStatement(cls);
            }
        }

        expect(TOK_RBRACE);

        classes[name] = cls;
    }

    void parseProperty(ActorClass* cls) {
        advance();  // property
        std::string propName = current.value;
        expect(TOK_IDENTIFIER);
        expect(TOK_EQUALS);
        int value = std::stoi(current.value);
        expect(TOK_NUMBER);
        expect(TOK_SEMICOLON);
        cls->properties[propName] = value;
    }

    void parseState(ActorClass* cls) {
        advance();  // state
        std::string stateName = current.value;
        expect(TOK_IDENTIFIER);

        State* state = new State{stateName, {}, 0, "", false};

        expect(TOK_LBRACE);

        while (current.type != TOK_RBRACE) {
            // Parse actions (e.g., A_Fire, or CON commands)
            std::string action = current.value;
            advance();
            // ... parse parameters, etc.
            state->actions.push_back(action);
            if (current.type == TOK_SEMICOLON) advance();
        }

        expect(TOK_RBRACE);

        cls->states[stateName] = state;
    }

    void parseFunction(ActorClass* cls) {
        advance();  // function
        std::string funcName = current.value;
        expect(TOK_IDENTIFIER);

        Function* func = new Function{funcName, {}, {}};

        expect(TOK_LPAREN);
        while (current.type != TOK_RPAREN) {
            func->params.push_back(current.value);
            expect(TOK_IDENTIFIER);
            if (current.type == TOK_COMMA) advance();
        }
        expect(TOK_RPAREN);

        expect(TOK_LBRACE);
        while (current.type != TOK_RBRACE) {
            parseStatement(func);
        }
        expect(TOK_RBRACE);

        cls->functions[funcName] = func;
    }

    void parseEvent(ActorClass* cls) {
        advance();  // event
        std::string eventName = current.value;
        expect(TOK_IDENTIFIER);
        cls->events.push_back(eventName);
        // Parse handler body if needed
    }

    void parsePropertyPatch() {
        advance();  // property
        std::string targetClass = current.value;
        expect(TOK_IDENTIFIER);
        std::string propName = current.value;
        expect(TOK_IDENTIFIER);
        expect(TOK_EQUALS);
        int value = std::stoi(current.value);
        expect(TOK_NUMBER);
        expect(TOK_SEMICOLON);

        if (classes.count(targetClass)) {
            classes[targetClass]->properties[propName] = value;
        } else {
            std::cerr << "Unknown class for patch: " << targetClass << std::endl;
        }
    }

    void parseStatement(void* context) {
        // Handle if/while/return/classic CON ops
        // For simplicity, collect lines
        std::string stmt;
        while (current.type != TOK_SEMICOLON && current.type != TOK_RBRACE) {
            stmt += current.value + " ";
            advance();
        }
        if (current.type == TOK_SEMICOLON) advance();

        if (auto* func = static_cast<Function*>(context)) {
            func->body.push_back(stmt);
        } else if (auto* cls = static_cast<ActorClass*>(context)) {
            // Add to default constructor or something
        }
    }
};

// VM for runtime (stub - extend with bytecode compilation)
class CONScriptVM {
public:
    void loadScript(const std::string& filename) {
        std::ifstream file(filename);
        std::stringstream buffer;
        buffer << file.rdbuf();
        Parser parser(buffer.str());
        if (parser.parse()) {
            actorClasses = parser.getClasses();
        }
    }

    void spawnActor(const std::string& className, int spriteIndex) {
        if (actorClasses.count(className)) {
            // Apply properties to sprite
            auto* cls = actorClasses[className];
            // e.g., tsprite[spriteIndex].health = cls->properties["health"];
            // Enter initial state
            executeState(cls->states["Spawn"], spriteIndex);
        }
    }

    void executeState(State* state, int spriteIndex) {
        // Run actions, wait duration, goto next
        // Integrate with game loop
    }

private:
    std::unordered_map<std::string, ActorClass*> actorClasses;
};

// Integration example (call from EDuke32's init or load)
CONScriptVM g_conscriptVM;

void init_conscript() {
    g_conscriptVM.loadScript("mod.conscript");
}

// In actor spawn (e.g., A_SpawnActor("MyActor"))
// g_conscriptVM.spawnActor("MyActor", i);

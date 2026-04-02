#define HOXML_IMPLEMENTATION
#include "hoxml.h"
#include "mock_ui.h"
#include "lualib.h"
#include <stdio.h>

// A simple no-op function for mocks that don't need to do anything.
static int l_noop(lua_State* L) {
    (void)L; // Unused
    return 0;
}

static int l_noop_empty_string(lua_State* L) {
    (void)L;
    lua_pushstring(L, "");
    return 1;
}

static int l_UI_CreateDataContext(lua_State* L) {
    // Simply return the passed table as the data context reference
    if (lua_istable(L, 1)) {
        lua_pushvalue(L, 1);
        return 1;
    }
    lua_newtable(L);
    return 1;
}

static int l_UI_AddChild(lua_State* L) {
    // function signature: UI_AddChild(parent, type, name, config_table)
    const char* name = luaL_checkstring(L, 3);
    
    // config_table is at index 4
    if (lua_istable(L, 4)) {
        lua_getfield(L, 4, "Xaml");
        if (lua_isstring(L, -1)) {
            size_t xaml_len;
            const char* xaml_content = lua_tolstring(L, -1, &xaml_len);
            
            hoxml_context_t ctx;
            char buffer[8192];
            hoxml_init(&ctx, buffer, sizeof(buffer));

            hoxml_code_t result = hoxml_parse(&ctx, xaml_content, xaml_len);
            
            while(result > 0) {
                result = hoxml_parse(&ctx, ctx.xml, ctx.xml_length);
            }

            if (result < 0 && result != HOXML_ERROR_UNEXPECTED_EOF) {
                char err_buf[256];
                const char* err_str = "Unknown error";
                switch(result) {
                    case HOXML_ERROR_INVALID_INPUT: err_str = "Invalid input"; break;
                    case HOXML_ERROR_INTERNAL: err_str = "Internal hoxml error"; break;
                    case HOXML_ERROR_INSUFFICIENT_MEMORY: err_str = "Insufficient memory"; break;
                    case HOXML_ERROR_SYNTAX: err_str = "Syntax error"; break;
                    case HOXML_ERROR_ENCODING: err_str = "Encoding error"; break;
                    case HOXML_ERROR_TAG_MISMATCH:
                        snprintf(err_buf, sizeof(err_buf), "Tag mismatch, expected closing tag for '%s'", ctx.tag ? ctx.tag : "unknown");
                        err_str = err_buf;
                        break;
                    case HOXML_ERROR_INVALID_DOCUMENT_TYPE_DECLARATION: err_str = "Invalid DOCTYPE"; break;
                    case HOXML_ERROR_INVALID_DOCUMENT_DECLARATION: err_str = "Invalid XML declaration"; break;
                }
                return luaL_error(L, "Failed to parse XAML (line %d, col %d): %s", ctx.line, ctx.column, err_str);
            }
        }
        lua_pop(L, 1); // pop Xaml string
        
        // Store the UI element in the registry
        lua_getfield(L, LUA_REGISTRYINDEX, "ScarTest_UI_State");
        if (lua_istable(L, -1)) {
            lua_pushvalue(L, 4); // The config table
            lua_setfield(L, -2, name);
        }
        lua_pop(L, 1); // pop registry table
    }
    return 0;
}

static int l_UI_SetDataContext(lua_State* L) {
    // function signature: UI_SetDataContext(name, context_table)
    const char* name = luaL_checkstring(L, 1);
    
    lua_getfield(L, LUA_REGISTRYINDEX, "ScarTest_UI_State");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, name);
        if (lua_istable(L, -1)) {
            lua_pushvalue(L, 2); // the context table passed in argument 2
            lua_setfield(L, -2, "DataContext");
        }
        lua_pop(L, 1); // pop UI element table
    }
    lua_pop(L, 1); // pop registry table
    
    return 0;
}

static int l_UI_CreateCommand(lua_State* L) {
    // Return a dummy command object (a new table)
    lua_newtable(L);
    return 1;
}

static int l_Mock_GetUIElement(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, "ScarTest_UI_State");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, name);
    } else {
        lua_pushnil(L);
    }
    // Leaves the requested element (or nil) on the stack, we return it
    return 1;
}

static const struct luaL_Reg ui_funcs[] = {
    {"UI_AddChild", l_UI_AddChild},
    {"UI_CreateCommand", l_UI_CreateCommand},
    {"UI_CreateDataContext", l_UI_CreateDataContext},
    {"UI_SetDataContext", l_UI_SetDataContext},
    {"UI_AddEventHandler", l_noop},
    {"UI_SetModal", l_noop},
    {"UI_ClearModal", l_noop},
    {"UI_RemoveChild", l_noop},
    {"UI_SetPropertyValue", l_noop},
    {"UI_SetDataTemplate", l_noop},
    {"UI_BindPropertyValue", l_noop},
    {"UI_DestroyDataContext", l_noop},
    {"UI_TitleDestroy", l_noop},
    {"UI_GetPropertyValue", l_noop_empty_string},
    
    // Mock helpers
    {"Mock_GetUIElement", l_Mock_GetUIElement},
    {NULL, NULL}
};

void mock_ui_register(lua_State* L) {
    // Initialize the UI state registry table
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "ScarTest_UI_State");

    lua_getglobal(L, "_G");
    luaL_setfuncs(L, ui_funcs, 0);
    lua_pop(L, 1);
}

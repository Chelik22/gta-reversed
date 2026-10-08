#include "StdInc.h"

#include "rtdict.h"


// ==== RW-CAMPAIGN wave-A shared decls (reconstructed from agent notes; pattern of rwplcore.cpp preamble) ====
// RtDict needs only stringFuncs.vecStrcmp (engine+0x114) and the malloc-style alloc/free pair
// (engine+0x134/+0x138). GTA-binary RwGlobals tail layout, decompile-verified (0x7CED40 family).
struct RwGlobalsGta
{
    RwUInt8     pad0[0x114];                                                // +0x000
    vecStrcmpFunc vecStrcmp;                                                // +0x114 (stringFuncs starts +0xF0)
    RwUInt8     pad1[0x134 - 0x118];                                        // +0x118..0x133
    void*       (*memoryAlloc)(RwUInt32 size, RwUInt32 hint);               // +0x134
    void        (*memoryFree)(void* mem);                                   // +0x138
};
static_assert(sizeof(RwGlobalsGta) == 0x13C, "RwGlobalsGta layout");
static RwGlobalsGta& RwGlobalsGtaGet()
{
    return *reinterpret_cast<RwGlobalsGta*>(RwEngineInstance);
}




RwBool RtDictSchemaDestruct(RtDictSchema* schema)
{
    _rwSListDestroy(schema->dictionaries);
    return TRUE;
}


RtDict* RtDictSchemaStreamReadDict(RtDictSchema* schema, RwStream* stream)
{
    RwGlobalsGta& globals = RwGlobalsGtaGet();

    RwUInt32 length;
    RwUInt32 version;
    if (!RwStreamFindChunk(stream, 1 /* rwID_STRUCT */, &length, &version))
    {
        return NULL;
    }

    if (version < schema->compatibilityVersion)
    {
        return schema->streamReadCompatibilityCB(stream);
    }

    RwUInt32 numEntries;
    if (!RwStreamRead(stream, &numEntries, length))
    {
        return NULL;
    }

    RtDict* dict = (RtDict*)globals.memoryAlloc(8 /* sizeof(RtDict) */, schema->dictChunkType | 0x30000);
    if (dict == NULL)
    {
        RwError rwError;
        rwError.pluginID  = 0x1BF; /* rtdict module rwPLUGIN_ID in the SA build (decompile uStack_8) */
        rwError.errorCode = _rwerror(0x80000013 /* E_RW_NOMEM */, 8);
        RwErrorSet(&rwError);
        return NULL;
    }

    dict->entries = _rwSListCreate(4 /* sizeof(RtDictEntryType) */, schema->dictChunkType | 0x30000);
    if (dict->entries != NULL)
    {
        dict->schema = schema;

        RtDict** slot = (RtDict**)_rwSListGetNewEntry(schema->dictionaries, schema->dictChunkType | 0x30000);
        if (slot != NULL)
        {
            *slot = dict;
            if (numEntries == 0)
            {
                return dict;
            }

            while (TRUE)
            {
                numEntries--;
                if (!RwStreamFindChunk(stream, schema->entryChunkType, &length, &version))
                {
                    break;
                }

                RtDictEntryType entry = schema->streamReadCB(stream);
                if (entry == NULL)
                {
                    break;
                }

                RtDictEntryType* eSlot = (RtDictEntryType*)_rwSListGetNewEntry(dict->entries,
                                                                               dict->schema->dictChunkType | 0x30000);
                *eSlot = entry;
                dict->schema->addRefCB(entry);
                schema->destroyCB(entry); /* same schema as dict->schema */

                if (numEntries == 0)
                {
                    return dict;
                }
            }

            /* failure: unlink from the schema (same sequence as RtDictDestroy), then destroy */
            if (dict == schema->current)
            {
                schema->current = NULL;
            }
            RtDict** end = (RtDict**)_rwSListGetEnd(schema->dictionaries);
            RtDict** it  = (RtDict**)_rwSListGetBegin(schema->dictionaries);
            while (it != end)
            {
                if (dict == *it)
                {
                    *it = *(RtDict**)_rwSListGetEntry(schema->dictionaries,
                                                      _rwSListGetNumEntries(schema->dictionaries) - 1);
                    _rwSListDestroyEndEntries(schema->dictionaries, 1);
                    break;
                }
                ++it;
            }

            RtDictEntryType* eEnd = (RtDictEntryType*)_rwSListGetEnd(dict->entries);
            RtDictEntryType* eIt  = (RtDictEntryType*)_rwSListGetBegin(dict->entries);
            for (; eIt != eEnd; ++eIt)
            {
                dict->schema->destroyCB(*eIt);
            }
            _rwSListDestroy(dict->entries);
            globals.memoryFree(dict);
            return NULL;
        }

        _rwSListDestroy(dict->entries);
    }

    globals.memoryFree(dict);
    return NULL;
}


RtDict* RtDictSchemaCreateDict(RtDictSchema* schema)
{
    RwGlobalsGta& globals = RwGlobalsGtaGet();

    RtDict* dict = (RtDict*)globals.memoryAlloc(8 /* sizeof(RtDict) */, schema->dictChunkType | 0x30000);
    if (dict == NULL)
    {
        RwError rwError;
        rwError.pluginID  = 0x1BF; /* rtdict module rwPLUGIN_ID in the SA build (decompile uStack_8) */
        rwError.errorCode = _rwerror(0x80000013 /* E_RW_NOMEM */, 8);
        RwErrorSet(&rwError);
        return NULL;
    }

    dict->entries = _rwSListCreate(4 /* sizeof(RtDictEntryType) */, schema->dictChunkType | 0x30000);
    if (dict->entries != NULL)
    {
        dict->schema = schema;

        RtDict** slot = (RtDict**)_rwSListGetNewEntry(schema->dictionaries, schema->dictChunkType | 0x30000);
        if (slot != NULL)
        {
            *slot = dict;
            return dict;
        }
        _rwSListDestroy(dict->entries);
    }

    globals.memoryFree(dict);
    return NULL;
}


RtDictSchema* RtDictSchemaAddDict(RtDictSchema* schema, RtDict* dict)
{
    RtDict** slot = (RtDict**)_rwSListGetNewEntry(schema->dictionaries, schema->dictChunkType | 0x30000);
    if (slot == NULL)
    {
        return NULL;
    }
    *slot = dict;
    return schema;
}


RtDictSchema* RtDictSchemaRemoveDict(RtDictSchema* schema, RtDict* dict)
{
    RtDict** end = (RtDict**)_rwSListGetEnd(schema->dictionaries);
    RtDict** it  = (RtDict**)_rwSListGetBegin(schema->dictionaries);
    while (it != end)
    {
        if (dict == *it)
        {
            *it = *(RtDict**)_rwSListGetEntry(schema->dictionaries, _rwSListGetNumEntries(schema->dictionaries) - 1);
            _rwSListDestroyEndEntries(schema->dictionaries, 1);
            return schema;
        }
        ++it;
    }
    return NULL;
}


RwBool RtDictSchemaForAllDictionaries(RtDictSchema* schema, RtDictCallBack* callback, RtDictEntryType data)
{
    RtDict** end = (RtDict**)_rwSListGetEnd(schema->dictionaries);
    RtDict** it  = (RtDict**)_rwSListGetBegin(schema->dictionaries);
    while (it != end)
    {
        RtDict* dict = *it;
        ++it;
        if (callback(dict, data) == NULL)
        {
            break;
        }
    }
    return TRUE;
}


RtDict* RtDictSchemaGetCurrentDict(RtDictSchema* schema)
{
    return schema->current;
}


RtDictSchema* RtDictSchemaSetCurrentDict(RtDictSchema* schema, RtDict* dict)
{
    schema->current = dict;
    return schema;
}


RwBool RtDictDestroy(RtDict* dictionary)
{
    RwGlobalsGta& globals = RwGlobalsGtaGet();
    RtDictSchema* schema  = dictionary->schema;

    if (dictionary == schema->current)
    {
        schema->current = NULL;
    }

    RtDict** end = (RtDict**)_rwSListGetEnd(schema->dictionaries);
    RtDict** it  = (RtDict**)_rwSListGetBegin(schema->dictionaries);
    while (it != end)
    {
        if (dictionary == *it)
        {
            *it = *(RtDict**)_rwSListGetEntry(schema->dictionaries, _rwSListGetNumEntries(schema->dictionaries) - 1);
            _rwSListDestroyEndEntries(schema->dictionaries, 1);
            break;
        }
        ++it;
    }

    RtDictEntryType* eEnd = (RtDictEntryType*)_rwSListGetEnd(dictionary->entries);
    RtDictEntryType* eIt  = (RtDictEntryType*)_rwSListGetBegin(dictionary->entries);
    for (; eIt != eEnd; ++eIt)
    {
        schema->destroyCB(*eIt);
    }
    _rwSListDestroy(dictionary->entries);
    globals.memoryFree(dictionary);
    return TRUE;
}


RtDictEntryType RtDictAddEntry(RtDict* dictionary, RtDictEntryType entry)
{
    RtDictEntryType* slot =
        (RtDictEntryType*)_rwSListGetNewEntry(dictionary->entries, dictionary->schema->dictChunkType | 0x30000);
    *slot = entry;
    dictionary->schema->addRefCB(entry);
    return (RtDictEntryType)dictionary; /* faithful to binary 0x7CEFB0 (returns dict, not entry) */
}


RtDictEntryType RtDictFindNamedEntry(RtDict* dictionary, const RwChar* name)
{
    RwGlobalsGta&               globals    = RwGlobalsGtaGet();
    RtDictEntryGetNameCallBack* getNameCB  = dictionary->schema->getNameCB;

    RtDictEntryType* end = (RtDictEntryType*)_rwSListGetEnd(dictionary->entries);
    RtDictEntryType* it  = (RtDictEntryType*)_rwSListGetBegin(dictionary->entries);
    while (it != end)
    {
        RtDictEntryType entry = *it;
        if (globals.vecStrcmp(getNameCB(entry), name) == 0)
        {
            return entry;
        }
        ++it;
    }
    return NULL;
}


const RtDict* RtDictForAllEntries(const RtDict* dictionary, RtDictEntryCallBack* callBack, RtDictEntryType data)
{
    RwSList*         entries = (RwSList*)dictionary->entries;
    RtDictEntryType* end     = (RtDictEntryType*)_rwSListGetEnd(entries);
    RtDictEntryType* it      = (RtDictEntryType*)_rwSListGetBegin(entries);
    while (it != end)
    {
        RtDictEntryType entry = *it;
        ++it;
        if (callBack(entry, data) == NULL)
        {
            break;
        }
    }
    return dictionary;
}


RtDict* RtDictRemoveEntry(RtDict* dictionary, RtDictEntryType entry)
{
    RtDictSchema*    schema = dictionary->schema;
    RtDictEntryType* end    = (RtDictEntryType*)_rwSListGetEnd(dictionary->entries);
    RtDictEntryType* it     = (RtDictEntryType*)_rwSListGetBegin(dictionary->entries);
    while (it != end)
    {
        if (*it == entry)
        {
            schema->destroyCB(*it);
            *it = *(RtDictEntryType*)_rwSListGetEntry(dictionary->entries,
                                                      _rwSListGetNumEntries(dictionary->entries) /* no -1 in binary */);
            _rwSListDestroyEndEntries(dictionary->entries, 1);
            return dictionary;
        }
        ++it;
    }
    return NULL;
}


const RtDict* RtDictStreamWrite(const RtDict* dictionary, RwStream* stream)
{
    RtDictSchema* schema  = dictionary->schema;
    RwSList*      entries = (RwSList*)dictionary->entries;

    RwUInt32 size = 0x10;
    RtDictEntryType* end = (RtDictEntryType*)_rwSListGetEnd(entries);
    RtDictEntryType* it  = (RtDictEntryType*)_rwSListGetBegin(entries);
    if (it != end)
    {
        for (; it != end; ++it)
        {
            size += 0xC + schema->streamGetSizeCB(*it);
        }
        if (size == 0)
        {
            return NULL;
        }
    }

    if (_rwStreamWriteVersionedChunkHeader(stream, (RwInt32)schema->dictChunkType, (RwInt32)size, 0x36003, 0xFFFF) == NULL)
    {
        return NULL;
    }
    if (_rwStreamWriteVersionedChunkHeader(stream, 1 /* rwID_STRUCT */, 4, 0x36003, 0xFFFF) == NULL)
    {
        return NULL;
    }

    RwUInt32 count = 0;
    end = (RtDictEntryType*)_rwSListGetEnd(entries);
    for (it = (RtDictEntryType*)_rwSListGetBegin(entries); it != end; ++it)
    {
        count++;
    }
    if (RwStreamWrite(stream, &count, 4) == NULL)
    {
        return NULL;
    }

    end = (RtDictEntryType*)_rwSListGetEnd(entries);
    for (it = (RtDictEntryType*)_rwSListGetBegin(entries); it != end; ++it)
    {
        if (schema->streamWriteCB(*it, stream) == NULL)
        {
            return NULL;
        }
    }
    return dictionary;
}


RwUInt32 RtDictStreamGetSize(const RtDict* dictionary)
{
    RtDictSchema* schema = dictionary->schema;
    RwSList*      entries = (RwSList*)dictionary->entries;

    RwUInt32 size = 0x10;
    RtDictEntryType* end = (RtDictEntryType*)_rwSListGetEnd(entries);
    RtDictEntryType* it  = (RtDictEntryType*)_rwSListGetBegin(entries);
    for (; it != end; ++it)
    {
        size += 0xC + schema->streamGetSizeCB(*it);
    }
    return size;
}


RtDict* _rtDictSchemaInitDict(RtDictSchema* schema, RtDict* dictionary)
{
    dictionary->entries = _rwSListCreate(4 /* sizeof(RtDictEntryType) */, schema->dictChunkType | 0x30000);
    if (dictionary->entries == NULL)
    {
        return NULL;
    }
    dictionary->schema = schema;

    RtDict** slot = (RtDict**)_rwSListGetNewEntry(schema->dictionaries, schema->dictChunkType | 0x30000);
    if (slot != NULL)
    {
        *slot = dictionary;
        return dictionary;
    }

    _rwSListDestroy(dictionary->entries);
    return NULL;
}


RwBool _rtDictDestruct(RtDict* dictionary)
{
    RtDictSchema*    schema = dictionary->schema;
    RtDictEntryType* end    = (RtDictEntryType*)_rwSListGetEnd(dictionary->entries);
    RtDictEntryType* it     = (RtDictEntryType*)_rwSListGetBegin(dictionary->entries);
    for (; it != end; ++it)
    {
        schema->destroyCB(*it);
    }
    _rwSListDestroy(dictionary->entries);
    return TRUE;
}


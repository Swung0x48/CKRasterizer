#ifndef XHASHTABLE_H
#define XHASHTABLE_H

#include "XClassArray.h"
#include "XArray.h"
#include "XSArray.h"
#include "XHashFun.h"

#ifdef VX_MSVC
#pragma warning(disable : 4786)
#endif

const float L = 0.75f;

template <class T, class K, class H, class Eq>
class XHashTable;

template <class T, class K>
class XHashTableEntry
{
    typedef XHashTableEntry<T, K> *tEntry;

public:
    XHashTableEntry(const K &k, const T &v) : m_Key(k), m_Data(v), m_Next(0) {}
    XHashTableEntry(const XHashTableEntry<T, K> &e) : m_Key(e.m_Key), m_Data(e.m_Data), m_Next(0) {}
    ~XHashTableEntry() {}

    K m_Key;
    T m_Data;
    tEntry m_Next;
};

/************************************************
Summary: Iterator on a hash table.

Remarks: This iterator is the only way to iterate on
elements in a hash table. The iteration will be in no
specific order, not in the insertion order. Here is an example
of how to use it:

Example:

    XHashTableIt<T,K,H> it = hashtable.Begin();
    while (it != hashtable.End()) {
        // access to the key
        it.GetKey();

        // access to the element
        *it;

        // next element
        ++it;
    }


************************************************/
template <class T, class K, class H = XHashFun<K>, class Eq = XEqual<K> >
class XHashTableIt {
    typedef XHashTableEntry <T, K> *tEntry;
    typedef XHashTableIt<T, K, H, Eq> tIterator;
    typedef XHashTable <T, K, H, Eq> *tTable;

    friend class XHashTable<T, K, H, Eq>;
public:
    /************************************************
    Summary: Default constructor of the iterator.
    ************************************************/
    XHashTableIt() : m_Node(0), m_Table(0) {}

    /************************************************
    Summary: Copy constructor of the iterator.
    ************************************************/
    XHashTableIt(const tIterator &n) : m_Node(n.m_Node), m_Table(n.m_Table) {}

    /************************************************
    Summary: Operator Equal of the iterator.
    ************************************************/
    int operator==(const tIterator &it) const { return m_Node == it.m_Node; }

    /************************************************
    Summary: Operator Not Equal of the iterator.
    ************************************************/
    int operator!=(const tIterator &it) const { return m_Node != it.m_Node; }

    /************************************************
    Summary: Returns a constant reference on the data
    pointed	by the iterator.

    Remarks:
        The returned reference is constant, so you can't
    modify its value. Use the other * operator for this
    purpose.
    ************************************************/
    const T &operator*() const { return (*m_Node).m_Data; }

    /************************************************
    Summary: Returns a reference on the data pointed
    by the iterator.

    Remarks:
        The returned reference is not constant, so you
    can modify its value.
    ************************************************/
    T &operator*() { return (*m_Node).m_Data; }

    /************************************************
    Summary: Returns a pointer on a T object.
    ************************************************/
    operator const T *() const { return &(m_Node->m_Data); }

    /************************************************
    Summary: Returns a pointer on a T object.
    ************************************************/
    operator T *() { return &(m_Node->m_Data); }

    /************************************************
    Summary: Returns a const reference on the key of
    the pointed entry.
    ************************************************/
    const K &GetKey() const { return m_Node->m_Key; }
    K &GetKey() { return m_Node->m_Key; }

    /************************************************
    Summary: Jumps to next entry in the hashtable.
    ************************************************/
    tIterator &operator++()
    { // Prefixe
        tEntry old = m_Node;
        // next element of the linked list
        m_Node = m_Node->m_Next;

        if (!m_Node)
        {
            // end of linked list, we have to find next filled bucket
            // OPTIM : maybe keep the index current : save a %
            int index = m_Table->Index(old->key);
            while (!m_Node && ++index < m_Table->m_Table.Size())
                m_Node = m_Table->m_Table[index];
        }
        return *this;
    }

    /************************************************
    Summary: Jumps to next entry in the hashtable.
    ************************************************/
    tIterator operator++(int)
    {
        tIterator tmp = *this;
        ++*this;
        return tmp;
    }

    XHashTableIt(tEntry n, tTable t) : m_Node(n), m_Table(t) {}

    tEntry m_Node;

    tTable m_Table;
};


/************************************************
Summary: Constant Iterator on a hash table.

Remarks: This iterator is the only way to iterate on
elements in a constant hash table. The iteration will be in no
specific order, not in the insertion order. Here is an example
of how to use it:

Example:

    void MyClass::MyMethod() const
    {
        XHashTableConstIt<T,K,H> it = m_Hashtable.Begin();
        while (it != m_Hashtable.End()) {
            // access to the key
            it.GetKey();

            // access to the element
            *it;

            // next element
            ++it;
        }
    }


************************************************/
template <class T, class K, class H = XHashFun<K>, class Eq = XEqual<K> >
class XHashTableConstIt
{
    typedef XHashTableEntry<T, K> *tEntry;
    typedef XHashTableConstIt<T, K, H, Eq> tConstIterator;
    typedef XHashTable<T, K, H, Eq> const *tConstTable;
    friend class XHashTable<T, K, H, Eq>;

public:
    /************************************************
    Summary: Default constructor of the iterator.
    ************************************************/
    XHashTableConstIt() : m_Node(0), m_Table(0) {}

    /************************************************
    Summary: Copy constructor of the iterator.
    ************************************************/
    XHashTableConstIt(const tConstIterator &n) : m_Node(n.m_Node), m_Table(n.m_Table) {}

    /************************************************
    Summary: Operator Equal of the iterator.
    ************************************************/
    int operator==(const tConstIterator &it) const { return m_Node == it.m_Node; }

    /************************************************
    Summary: Operator Not Equal of the iterator.
    ************************************************/
    int operator!=(const tConstIterator &it) const { return m_Node != it.m_Node; }

    /************************************************
    Summary: Returns a constant reference on the data
    pointed	by the iterator.

    Remarks:
        The returned reference is constant, so you can't
    modify its value. Use the other * operator for this
    purpose.
    ************************************************/
    const T &operator*() const { return (*m_Node).m_Data; }

    /************************************************
    Summary: Returns a pointer on a T object.
    ************************************************/
    operator const T *() const { return &(m_Node->m_Data); }

    /************************************************
    Summary: Returns a const reference on the key of
    the pointed entry.
    ************************************************/
    const K &GetKey() const { return m_Node->m_Key; }

    /************************************************
    Summary: Jumps to next entry in the hashtable.
    ************************************************/
    tConstIterator &operator++()
    { // Prefixe
        tEntry old = m_Node;
        // next element of the linked list
        m_Node = m_Node->m_Next;

        if (!m_Node)
        {
            // end of linked list, we have to find next filled bucket
            // OPTIM : maybe keep the index current : save a %
            int index = m_Table->Index(old->m_Key);
            while (!m_Node && ++index < m_Table->m_Table.Size())
                m_Node = m_Table->m_Table[index];
        }
        return *this;
    }

    /************************************************
    Summary: Jumps to next entry in the hashtable.
    ************************************************/
    tConstIterator operator++(int)
    {
        tConstIterator tmp = *this;
        ++*this;
        return tmp;
    }

    XHashTableConstIt(tEntry n, tConstTable t) : m_Node(n), m_Table(t) {}

    tEntry m_Node;

    tConstTable m_Table;
};

/************************************************
Summary: Struct containing an iterator on an object
inserted and a BOOL determining if it were really
inserted (TRUE) or already there (FALSE).


************************************************/
template <class T, class K, class H = XHashFun<K>, class Eq = XEqual<K> >
class XHashTablePair
{
public:
    XHashTablePair(XHashTableIt<T, K, H, Eq> it, int n) : m_Iterator(it), m_New(n){};

    XHashTableIt<T, K, H, Eq> m_Iterator;
    XBOOL m_New;
};

/************************************************
Summary: Class representation of an Hash Table
container.

Remarks:
    T is the type of element to insert
    K is the type of the key
    H is the hash function to hash the key

    Several hash functions for basic types are
already defined in XHashFun.h

    This implementation of the hash table uses
Linked List in each bucket for element hashed to
the same index, so there are memory allocation
for each insertion. For a static implementation
without dynamic allocation, look at XSHashTable.

    There is a m_LoadFactor member which allow the
user to decide at which occupation the hash table
must be extended and rehashed.


************************************************/
template <class T, class K, class H = XHashFun<K>, class Eq = XEqual<K> /*, float L = 0.75f*/>
class XHashTable
{
    // Types
    typedef XHashTable<T, K, H, Eq> tTable;
    typedef XHashTableEntry<T, K> *tEntry;
    typedef XHashTableIt<T, K, H, Eq> tIterator;
    typedef XHashTableConstIt<T, K, H, Eq> tConstIterator;
    typedef XHashTablePair<T, K, H, Eq> tPair;
    // Friendship
    friend class XHashTableIt<T, K, H, Eq>;
    // Friendship
    friend class XHashTableConstIt<T, K, H, Eq>;

public:
    typedef XHashTablePair<T, K, H, Eq> Pair;
    typedef XHashTableIt<T, K, H, Eq> Iterator;
    typedef XHashTableConstIt<T, K, H, Eq> ConstIterator;

    /************************************************
    Summary: Default Constructor.

    Input Arguments:
        initialize: The default number of buckets
        (should be a power of 2, otherwise will be
        converted.)
        l: Load Factor (see Class Description).
        a: hash table to copy.

    ************************************************/
    XHashTable(int initialize = 16)
    {
        initialize = Near2Power(initialize);
        if (initialize < 4)
            initialize = 4;

        m_Table.Resize(initialize);
        m_Table.Fill(0);
        m_Pool.Reserve((int)(initialize * L));
    }

    /************************************************
    Summary: Copy Constructor.
    ************************************************/
    XHashTable(const XHashTable &a) { XCopy(a);}

    /************************************************
    Summary: Destructor.

    Remarks:
        Release the elements contained in the hash table. If
    you were storing pointers, you need first to iterate
    on the table and call delete on each pointer.
    ************************************************/
    ~XHashTable() {}

    /************************************************
    Summary: Removes all the elements from the table.

    Remarks:
        The hash table remains with the same number
    of buckets after a clear.
    ************************************************/
    void Clear()
    {
        // we clear all the allocated entries
        m_Pool.Resize(0);
        // we clear the table
        m_Table.Fill(0);
    }

    /************************************************
    Summary: Calculates the average occupation for the
    buckets by filling an array with the population for
    different bucket size (represented by the index of
    the array)

    ************************************************/
    void GetOccupation(XArray<int> &iBucketOccupation) const
    {
        iBucketOccupation.Resize(1);
        iBucketOccupation[0] = 0;

        for (tEntry *it = m_Table.Begin(); it != m_Table.End(); it++)
        {
            if (!*it)
            { // there is someone there
                iBucketOccupation[0]++;
            }
            else
            {
                // count the number of occupant
                int count = 1;
                tEntry e = *it;
                while (e->m_Next)
                {
                    e = e->m_Next;
                    count++;
                }

                int oldsize = iBucketOccupation.Size();
                if (oldsize <= count)
                { // we need to resize
                    iBucketOccupation.Resize(count + 1);

                    // and we init to 0
                    for (int i = oldsize; i <= count; ++i)
                        iBucketOccupation[i] = 0;
                }

                // the recensing
                iBucketOccupation[count]++;
            }
        }
    }

    /************************************************
    Summary: Affectation operator.

    Remarks:
        The content of the table is entirely overwritten
    by the given table.
    ************************************************/
    tTable &operator=(const tTable &a)
    {
        if (this != &a)
        {
            // We clear the current table
            Clear();
            // we then copy the content of a
            XCopy(a);
        }

        return *this;
    }

    /************************************************
    Summary: Inserts an element in the table.

    Input Arguments:
        key: key of the element to insert.
        o: element to insert.
        override: if the key is already present, should
    the old element be overridden ?

    Remarks:
        Insert will automatically override the old value
    and InsertUnique will not replace the old value.
    TestInsert returns a XHashPair, which allow you to know
    if the element was already present.
    ************************************************/
    XBOOL Insert(const K &key, const T &o, XBOOL override)
    {
        int index = Index(key);

        // we look for existing key
        tEntry e = XFind(index, key);
        if (!e)
        {
            if (m_Pool.Size() == m_Pool.Allocated())
            { // Need Rehash
                Rehash(m_Table.Size() * 2);
                return Insert(key, o, override);
            }
            else
            { // No
                XInsert(index, key, o);
            }
        }
        else
        {
            if (!override)
                return FALSE;
            e->m_Data = o;
        }

        return TRUE;
    }

    Iterator Insert(const K &key, const T &o)
    {
        int index = Index(key);
        Eq equalFunc;

        // we look for existing key
        for (tEntry e = m_Table[index]; e != 0; e = e->next)
        {
            if (equalFunc(e->m_Key, key))
            {
                e->m_Data = o;
                return Iterator(e, this);
            }
        }

        if (m_Pool.Size() == m_Pool.Allocated())
        { // Need Rehash
            Rehash(m_Table.Size() * 2);
            return Insert(key, o);
        }
        else
        { // No
            return Iterator(XInsert(index, key, o), this);
        }
    }

    Pair TestInsert(const K &key, const T &o)
    {
        int index = Index(key);
        Eq equalFunc;

        // we look for existing key
        for (tEntry e = m_Table[index]; e != 0; e = e->next)
        {
            if (equalFunc(e->m_Key, key))
            {
                return Pair(Iterator(e, this), 0);
            }
        }

        // Need Rehash
        if (m_Pool.Size() == m_Pool.Allocated())
        { // Need Rehash
            Rehash(m_Table.Size() * 2);
            return TestInsert(key, o);
        }
        else
        { // No
            return Pair(Iterator(XInsert(index, key, o), this), 1);
        }
    }

    Iterator InsertUnique(const K &key, const T &o)
    {
        int index = Index(key);
        Eq equalFunc;

        // we look for existing key
        for (tEntry e = m_Table[index]; e != 0; e = e->next)
        {
            if (equalFunc(e->m_Key, key))
            {
                return Iterator(e, this);
            }
        }

        // Need Rehash
        if (m_Pool.Size() == m_Pool.Allocated())
        { // Need Rehash
            Rehash(m_Table.Size() * 2);
            return InsertUnique(key, o);
        }
        else
        { // No
            return Iterator(XInsert(index, key, o), this);
        }
    }

    /************************************************
    Summary: Removes an element.

    Input Arguments:
        key: key of the element to remove.
        it: iterator on the object to remove.

    Return Value: iterator on the element next to
    the one just removed.
    ************************************************/
    void Remove(const K &key)
    {
        int index = Index(key);
        Eq equalFunc;

        // we look for existing key
        tEntry old = NULL;
        for (tEntry e = m_Table[index]; e != 0; e = e->next)
        {
            if (equalFunc(e->m_Key, key))
            {
                // This is the element to remove

                // change the pointers to it
                if (old)
                {
                    old->m_Next = e->m_Next;
                }
                else
                {
                    m_Table[index] = e->m_Next;
                }

                // then removed it from the pool
                m_Pool.FastRemove(e);
                if (e != m_Pool.End())
                { // wasn't the last one... we need to remap
                    RematEntry(m_Pool.End(), e);
                }

                break;
            }
            old = e;
        }
    }

    Iterator Remove(const tIterator &it)
    {
        int index = Index(it.m_Node->key);
        if (index >= m_Table.Size())
            return Iterator(0, this);

        // we look for existing key
        tEntry old = NULL;
        for (tEntry e = m_Table[index]; e != 0; e = e->next)
        {
            if (e == it.m_Node)
            {
                // This is the element to remove
                if (old)
                {
                    old->m_Next = e->m_Next;
                    old = old->m_Next;
                }
                else
                {
                    m_Table[index] = e->m_Next;
                    old = m_Table[index];
                }

                // then removed it from the pool
                m_Pool.FastRemove(e);
                if (e != m_Pool.End())
                { // wasn't the last one... we need to remap
                    RematEntry(m_Pool.End(), e);
                    if (old == m_Pool.End())
                        old = e;
                }

                break;
            }
            old = e;
        }
        // There is an element in the same column, we return it
        if (!old)
        { // No element in the same bucket, we parse for the next
            while (!old && ++index < m_Table.Size())
                old = m_Table[index];
        }

        return Iterator(old, this);
    }

    /************************************************
    Summary: Access to an hash table element.

    Input Arguments:
        key: key of the element to access.

    Return Value: a copy of the element found.

    Remarks:
        If no element correspond to the key, an element
    constructed with 0.
    ************************************************/
    T &operator[](const K &key)
    {
        int index = Index(key);

        // we look for existing key
        tEntry e = XFind(index, key);
        if (!e)
        {
            if (m_Pool.Size() == m_Pool.Allocated())
            { // Need Rehash
                Rehash(m_Table.Size() * 2);
                return operator[](key);
            }
            else
            { // No
                e = XInsert(index, key, T());
            }
        }

        return e->m_Data;
    }

    /************************************************
    Summary: Access to an hash table element.

    Input Arguments:
        key: key of the element to access.

    Return Value: an iterator of the element found. End()
    if not found.

    ************************************************/
    Iterator Find(const K &key)
    {
        return Iterator(XFindIndex(key), this);
    }

    /************************************************
    Summary: Access to a constant hash table element.

    Input Arguments:
        key: key of the element to access.

    Return Value: a constant iterator of the element found. End()
    if not found.

    ************************************************/
    ConstIterator Find(const K &key) const
    {
        return ConstIterator(XFindIndex(key), this);
    }

    /************************************************
    Summary: Access to an hash table element.

    Input Arguments:
        key: key of the element to access.

    Return Value: a pointer on the element found. NULL
    if not found.

    ************************************************/
    T *FindPtr(const K &key) const
    {
        tEntry e = XFindIndex(key);
        if (e)
            return &e->m_Data;
        else
            return 0;
    }

    /************************************************
    Summary: search for an hash table element.

    Input Arguments:
        key: key of the element to access.
        value: value to receive the element found value.

    Return Value: TRUE if the key was found, FALSE
    otherwise..

    ************************************************/
    XBOOL LookUp(const K &key, T &value) const
    {
        tEntry e = XFindIndex(key);
        if (e)
        {
            value = e->m_Data;
            return TRUE;
        }
        else
            return FALSE;
    }

    /************************************************
    Summary: test for the presence of a key.

    Input Arguments:
        key: key of the element to access.

    Return Value: TRUE if the key was found, FALSE
    otherwise..

    ************************************************/
    XBOOL IsHere(const K &key) const
    {
        return XFindIndex(key) != NULL;
    }

    /************************************************
    Summary: Returns an iterator on the first element.

    Example:
        Typically, an algorithm iterating on an hash table
    looks like:

        XHashTable<T,K,H>::Iterator it = h.Begin();
        XHashTable<T,K,H>::Iterator itend = h.End();

        for(; it != itend; ++it) {
            // do something with *t
        }

    ************************************************/
    Iterator Begin()
    {
        for (tEntry *it = m_Table.Begin(); it != m_Table.End(); it++)
        {
            if (*it)
                return Iterator(*it, this);
        }
        return End();
    }

    ConstIterator Begin() const
    {
        for (tEntry *it = m_Table.Begin(); it != m_Table.End(); it++)
        {
            if (*it)
                return ConstIterator(*it, this);
        }
        return End();
    }

    /************************************************
    Summary: Returns an iterator out of the hash table.
    ************************************************/
    Iterator End()
    {
        return Iterator(0, this);
    }

    ConstIterator End() const
    {
        return ConstIterator(0, this);
    }

    /************************************************
    Summary: Returns the index of the given key.

    Input Arguments:
        key: key of the element to find the index.
    ************************************************/
    int Index(const K &key) const
    {
        H hashfun;
        return XIndex(hashfun(key), m_Table.Size());
    }

    /************************************************
    Summary: Returns the elements number.
    ************************************************/
    int Size() const
    {
        return m_Pool.Size();
    }

    /************************************************
    Summary: Return the occupied size in bytes.

    Parameters:
        addstatic: TRUE if you want to add the size occupied
    by the class itself.
    ************************************************/
    int GetMemoryOccupation(XBOOL addstatic = FALSE) const
    {
        return m_Table.GetMemoryOccupation(addstatic) +
               m_Pool.Allocated() * sizeof(tEntry) +
               (addstatic ? sizeof(*this) : 0);
    }

    /************************************************
    Summary: Reserve an estimation of future hash occupation.

    Parameters:
        iCount: count of elements to reserve
    Remarks:
        you need to call this function before populating
    the hash table.
    ************************************************/
    void Reserve(const int iCount)
    {
        // Reserve the elements
        m_Pool.Reserve(iCount);

        int tableSize = Near2Power((int)(iCount / L));
        m_Table.Resize(tableSize);
        m_Table.Fill(0);
    }

private:
    // Types

    ///
    // Methods

    tEntry *GetFirstBucket() const { return m_Table.Begin(); }

    void Rehash(int iSize)
    {
        int oldsize = m_Table.Size();

        // we create a new pool
        XClassArray<tEntry> pool((int)(iSize * L));
        pool = m_Pool;

        // Temporary table
        XSArray<tEntry> tmp;
        tmp.Resize(iSize);
        tmp.Fill(0);

        for (int index = 0; index < oldsize; ++index)
        {
            tEntry *first = m_Table[index];
            while (first)
            {
                H hashfun;
                int newindex = XIndex(hashfun(first->key), iSize);

                tEntry *newe = pool.Begin() + (first - m_Pool.Begin());

                // insert new entry in new table
                newe->m_Next = tmp[newindex];
                tmp[newindex] = newe;

                first = first->m_Next;
            }
        }
        m_Table.Swap(tmp);
        m_Pool.Swap(pool);
    }

    int XIndex(int key, int size) const
    {
        return key & (size - 1);
    }

    void XCopy(const XHashTable &a)
    {
        int size = a.m_Table.Size();
        m_Table.Resize(size);
        m_Table.Fill(0);

        m_Pool.Reserve(a.m_Pool.Allocated());
        m_Pool = a.m_Pool;

        // remap the address in the table
        for (int i = 0; i < size; ++i)
        {
            if (a.m_Table[i])
                m_Table[i] = m_Pool.Begin() + (a.m_Table[i] - a.m_Pool.Begin());
        }

        // remap the addresses in the entries
        for (tEntry *e = m_Pool.Begin(); e != m_Pool.End(); ++e)
        {
            if (e->m_Next)
            {
                e->m_Next = m_Pool.Begin() + (e->m_Next - a.m_Pool.Begin());
            }
        }
    }

    tEntry XFindIndex(const K &key) const
    {
        int index = Index(key);
        return XFind(index, key);
    }

    tEntry XFind(int index, const K &key) const
    {
        Eq equalFunc;

        // we look for existing key
        for (tEntry e = m_Table[index]; e != 0; e = e->m_Next)
        {
            if (equalFunc(e->m_Key, key))
            {
                return e;
            }
        }
        return NULL;
    }

    tEntry XInsert(int index, const K &key, const T &o)
    {
        tEntry *newe = GetFreeEntry();
        newe->m_Key = key;
        newe->m_Data = o;
        newe->m_Next = m_Table[index];
        m_Table[index] = newe;
        return newe;
    }

    tEntry GetFreeEntry()
    {
        // We consider when we arrive here that we have space
        m_Pool.Resize(m_Pool.Size() + 1);
        return (m_Pool.End() - 1);
    }

    void RematEntry(tEntry iOld, tEntry iNew)
    {
        int index = Index(iNew->m_Key);
        XASSERT(m_Table[index]);

        if (m_Table[index] == iOld)
        { // It was the first of the bucket
            m_Table[index] = iNew;
        }
        else
        {
            for (tEntry n = m_Table[index]; n->next != NULL; n = n->m_Next)
            {
                if (n->m_Next == iOld)
                { // found one
                    n->m_Next = iNew;
                    break; // only one can match
                }
            }
        }
    }

    ///
    // Members

    // the hash table data {secret}
    XArray<tEntry> m_Table;
    // the entry pool {secret}
    XClassArray<tEntry> m_Pool;
};

#endif // XHASHTABLE_H

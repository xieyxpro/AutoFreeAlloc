#ifndef _AUTOFREEALLOC_H_
#define _AUTOFREEALLOC_H_

const int MEMORY_BLOCK_SIZE = 2048 + 4   //2MB and HeaderSize(a pointer)

// -------------------------------------------------------------------------
// class AutoFreeAlloc

template <class _Alloc, int _MemBlockSize = MEMORY_BLOCK_SIZE>
class AutoFreeAllocT
{
public:
    enum { MemBlockSize = _MemBlockSize };
    enum { HeaderSize = sizeof(void*) };
    enum { BlockSize  = _MemBlockSize - HeaderSize };
private:
    struct _MemBlock
    {
        _MemBlock* pPrev;
        char buffer[BlockSize];     //2MB
    };

    typedef void (*FnDestructor)(void *pthis)
    struct _DestroyNode
    {
        _DestroyNode *pPrev;
        FnDestructor fnDestroy;
    };

    char* m_begin;
    char* m_end;
    _DestroyNode* m_destroyChain;
    _Alloc m_alloc;

private:
    //use to get Header
    _MemBlock* _ChainHeader() const
    {
        return (_MemBlock*)(m_begin - HeaderSize);
    }

    AutoFreeAllocT(const AutoFreeAllocT& rhs);
    const AutoFreeAllocT& operator=(const AutoFreeAllocT& rhs);

public:
    AutoFreeAllocT(): m_destroyChain(nullptr)
    {
        m_begin = m_end = (char*)HeaderSize;    //init chainHeader to null
    }
    
    AutoFreeAllocT(_Alloc alloc): m_alloc(alloc), m_destroyChain(nullptr)
    {
        m_begin = m_end = (char*)HeaderSize;
    }

    ~AutoFreeAllocT()
    {
        clear();
    }

    void swap(AutoFreeAllocT& o)
    {
        std::swap(m_begin, o.m_begin);
        std::swap(m_end, o.m_end);
        std::swap(m_destroyChain, o.m_destroyChain);
        m_alloc.swap(o.m_alloc);
    }

    void clear()
    {
        while (m_destroyChain)
        {
            m_destroyChain->fnDestroy(m_destroyChain + 1);
            m_destroyChain = m_destroyChain->pPrev;
        }
        _MemBlock* pHeader = _MemBlock* pHeader = _ChainHeader();
        while (pHeader)
        {
            _MemBlock* pTemp = pHeader->pPrev;
            m_alloc.deallocate(pHeader);
            pHeader = pTemp;
        }
        m_begin = m_end = (char*)HeaderSize;
    }

    void* winx_call allocate(size_t cb)
    {
        if ((size_t)(m_end - m_begin) < cb)
        {
            if (cb >= BlockSize)
            {
                _MemBlock* pHeader = _ChainHeader();
                _MemBlock* pNew = (_MemBlock*)m_alloc.allocate(HeaderSize + cb);
                if (pHeader)
                {
                    pNew->pPrev = pHeader->pPrev;
                    pHeader->pPrev = pNew;
                }
                else
                {
                    m_end = m_begin = pNew->buffer;
                    pNew->pPrev = NULL;
                }
                return pNew->buffer;
            }
            else
            {
                _MemBlock* pNew = (_MemBlock*)m_alloc.allocate(sizeof(_MemBlock));
                pNew->pPrev = _ChainHeader();
                m_begin = pNew->buffer;
                m_end = m_begin + BlockSize;
            }
        }
        return m_end -= cb;
    }

    void* allocate(size_t cb, DestructorType fn)
    {
        _DestroyNode* pNode = (_DestroyNode*)allocate(sizeof(_DestroyNode) + cb);
        pNode->fnDestroy = fn;
        pNode->pPrev = m_destroyChain;
        m_destroyChain = pNode;
        return pNode + 1;
    }
}

#endif

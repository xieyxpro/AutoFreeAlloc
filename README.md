# AutoFreeAlloc

C/C++最被人诟病的，可能是没有一个内存垃圾回收器（确切是说没有一个标准的垃圾回收器）。这个小玩意要点是，在C/C++中实现一个最袖珍的、功能受限的内存分配和垃圾回收器。这个垃圾回收器区别于其他垃圾回收器的主要特征是：

  1.  **袖珍但具实用性。** 整个垃圾回收器代码行数100行左右（不含空白行），相当小巧。相对而言，它的功能也受到一定的限制。但是它在很多关键的场合恰恰非常有用。该垃圾回收器以实用作为首要目标。
  2.  **高性能。** 区别于其他垃圾回收器的是这个袖珍的垃圾回收器非但不会导致性能的下降，反而提高了程序的时间性能（分配的速度加快）和空间性能（所占内存空间比正常的malloc/new少）。而这也是实用的重要指标。

## 思路
**这个内存分配和垃圾回收器的关键点在于，是在于理解它的目标：为一个复杂的局部过程（算法）提供自动内存回收的能力。**

局部过程（算法），是指那些算法复杂性较高，但在程序运行期所占的时间又比较短暂的过程。例如：搜索引擎的搜索过程、读盘/存盘过程、显示（绘制）过程等等。通常这些过程可能需要申请很多内存，而且内存分配操作的入口点很多（就是调用new的地方很多），如果每调用一次new就要考虑应该在什么地方delete就徒然浪费我们宝贵的脑力，使得我们无法把全力精力集中在算法本身的设计上。也许就是在这种情形下，C/C++程序员特别羡慕那些具备垃圾回收器的语言。相对而言，如果算法复杂性不高的话，我们的程序员完全有能力控制好new/delete的匹配关系。并且，这种“一切皆在我掌控之中”的感觉给了我们安全感和满足感。　

因此， **这个垃圾回收器的重心并不是要提供一个理论上功能完备的内存自动回收机制。它只是针对复杂性较高的局部过程（算法），为他们提供最实效的内存管理手段。从局部过程的一开始，你就只管去申请、使用内存，等到整个算法完成之后，这个过程申请的大部分内存（需要作为算法结果保留的例外），无论它是在算法的那个步骤申请的，均在这个结束点上由垃圾回收器自动销毁。** 示意图如下：

![image](http://cplusplus.wikidot.com/local--files/cn:auto-alloc/1.gif)

## 细节
我把这个分配器命名为 AutoAlloc。它的接口很简单，仅涉及两个概念：allocate、clear。
```
typedef void (*FnDestructor)(void* pThis);
class AutoAlloc
{
public:
    ~AutoAlloc();                                  // 析构函数。自动调用Clear释放内存
    void* allocate(size_t cb);                     // 类似于malloc(cb)
    void* allocate(size_t cb, FnDestructor fn);    // 申请内存并指定析构函数
    void clear();                                  // 析构并释放所有分配的对象
};
```

## 内存管理机制
```
class AutoAlloc
{
public:
    enum { BlockSize = 2048 };
private:
    struct _MemBlock
    {
        _MemBlock* pPrev;
        char buffer[BlockSize];
    };
    enum { HeaderSize = sizeof(_MemBlock) - BlockSize };
 
    char* m_begin;
    char* m_end;
};
```
AutoAlloc类与内存管理相关的变量只有两个：m_begin、m_end。单从变量定义来看，基本上很难看明白。示意图如下：
![image](http://cplusplus.wikidot.com/local--files/cn:auto-alloc/2.png)

整个AutoAlloc申请的内存，通过_MemBlock构成链表。只要获得了链表的头，就可以遍历整个内存链，释放所有申请的内存了。而链表的头(图中标为_ChainHeader)，可以通过m_begin计算得到：
```
_MemBlock* AutoAlloc::_ChainHeader() const
{
    return (_MemBlock*)(m_begin - HeaderSize);
}
```
为了使得_ChainHeader初始值为null，构造函数我们这样写：
```
AutoAlloc::AutoAlloc()
{
    m_begin = m_end = (char*)HeaderSize;
}
```
## 内存分配（Alloc）过程
Alloc过程主要会有三种情况，具体代码为:
```
void* AutoAlloc::Alloc(size_t cb)
{
    if (m_end – m_begin < cb)
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
                return pNew->buffer;        }
        else
        {
            _MemBlock* pNew = (_MemBlock*)malloc(sizeof(_MemBlock));
            pNew->pPrev = _ChainHeader();
            m_begin = pNew->buffer;
            m_end = m_begin + BlockSize;
        }
    }
    return m_end -= cb;
}
```

1. 也就是最简单的情况，当前_MemBlock还有足够的自由内存(free memory)，即：只需要将m_end前移cb字节就可以了。
![image](http://cplusplus.wikidot.com/local--files/cn:auto-alloc/3.png)

2. 在当前的_MemBlock的自由内存（free memory）不足的情况下，我们就需要申请一个新的_MemBlock以供使用。申请新的_MemBlock又会遇到两种情况：
  - 申请的字节数（即cb）小于一个_MemBlock所能够提供的内存（即BlockSize）。只需要将该_MemBlock作为新的当前_MemBlock挂到链表中，剩下的工作就和情形1完全类似。
  ![image](http://cplusplus.wikidot.com/local--files/cn:auto-alloc/4.png)
  - 而在内存申请的字节数（即cb）大于或等于一个Block的字节数时，我们需要申请可使用内存超过正常长度（BlockSize）的_MemBlock。这个新生成的_MemBlock全部内存被用户申请。故此，我们只需要修改_ChainHeader的pPrev指针，改为指向这一块新申请的_MemBlock即可。m_begin、m_end保持不变（当前的_MemBlock还是当前的_MemBlock）。
  ![image](http://cplusplus.wikidot.com/local--files/cn:auto-alloc/5.png)

## 内存释放（Clear）过程
考虑内存释放（Clear）过程。这个过程就是遍历_MemBlock释放所有的_MemBlock的过程，非常简单。
```
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
```

## 自动析构过程
由于垃圾回收器负责对象的回收，它自然不止需要关注对象申请的内存的释放，同时也需要保证，在对象销毁之前它的析构过程被调用。上文我们为了关注内存管理过程，把自动析构过程需要的代码均去除了。为了支持自动析构，AutoAlloc类增加了以下成员：
```
struct _DestroyNode
{
   _DestroyNode* pPrev;
   FnDestructor fnDestroy;
};
```
如果一个类存在析构，则它需要在Alloc内存的同时指定析构函数。
```
void* AutoAlloc::Alloc(size_t cb, FnDestructor fn)
{
    _DestroyNode* pNode = (_DestroyNode*)Alloc(sizeof(_DestroyNode) + cb);
    pNode->fnDestroy = fn;
    pNode->pPrev = m_destroyChain;
    m_destroyChain = pNode;
    return pNode + 1;
}
```
只要通过该Alloc函数申请的内存，我们在Clear中就可以调用相应的析构。
```
void AutoAlloc::Clear()
{
    while (m_destroyChain)
    {
        m_destroyChain->fnDestroy(m_destroyChain + 1);
        m_destroyChain = m_destroyChain->pPrev;
    }
    // 以下是原先正常的内存释放过程…
}
```

## 时间性能分析
以对象大小平均为32字节计算的话，每2048/32 = 64操作中，只有一次操作满足m_end – m_begin < cb的条件。也就是说，在通常情况（63/64 = 98.4%的概率）下，Alloc操作只需要一个减法操作就完成内存分配。

一般内存管理器通常一次内存分配操作就需调用相应的一次Free操作。但是AutoAlloc不针对每一个Alloc进行释放，而是针对每一个_MemBlock。仍假设对象平均大小为32字节的话，也就是相当于把64次Alloc操作合并，为其提供一次相应的Free过程。
结论：AutoAlloc在时间上的性能，大约比普通的malloc/free的快64倍。

## 空间性能分析
一般内存管理器为了将用户申请的内存块管理起来，除了用户需要的cb字节内存外，通常额外还提供一个内存块的头结构，通过这个头结构将内存串连成为一个链表。一般来讲，这个头结构至少有两项（可能还不止），示意如下：
```
struct MemHeader
{
    MemHeader* pPrev;
    size_t cb;
};
```
仍然假设平均Alloc一次的内存为32字节。则一次malloc分配过程，就会浪费8/32 = 25%的内存。并且由于大量的小对象存在，整个内存中的碎片（指那些自由但无法被使用的内存）将特别严重。

而AutoAlloc的Alloc没有如何额外开销。整个AutoAlloc，只有在将_MemBlock串为链表的有一个额外的 pPrev指针，加上_MemBlock是malloc出来的，有额外的8字节开销。总计浪费(4+8)/2048 = 0.6%的内存，几乎可以忽略不计。

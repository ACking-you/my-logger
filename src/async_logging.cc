//
// Created by Alone on 2022-9-21.
//

#include <cassert>
#include <chrono>
#include <utility>

#include "elog/log_file.h"
#include "elog/trace.impl.h"
#include "elog/logger_util.h"

USING_LBLOG
USING_LBLOG_DETAIL

AsyncLogging::AsyncLogging(const char* basename, int rollSize,
                           int flushInterval)
    : m_basename(basename),
      m_rollSize(rollSize),
      m_flushInterval(flushInterval),
      m_done(false),
      m_latch(1)
{
    // 由于构造函数抛出异常不会调用析构，所以需要做异常安全处理
    try
    {
        m_curBuffer  = elog::make_unique<Buffer>();
        m_nextBuffer = elog::make_unique<Buffer>();

        LB_TRACE_("后台线程任务开始创建");
        m_thread =
            elog::make_unique<std::thread>([this]() { thread_worker(); });
        LB_TRACE_("开始等待后台线程资源初始化");
        m_latch.wait(); // 等待线程任务的资源初始化完成
        LB_TRACE_("线程任务资源初始化完成");
    }
    catch (...)
    {
        LB_TRACE_("创建线程任务或buffer初始化失败");
        do_done(); // 需要做的额外安全处理
        throw std::runtime_error(
            "AsyncLogging create thread or buffer alloc error");
    }
}

AsyncLogging::~AsyncLogging()
{
    LB_TRACE_("AsyncLogging析构执行，进行资源的清理");
    do_done();
}

// 如果发生异常，则需要维护最后的资源安全退出
void AsyncLogging::do_done()
{
    if (m_done)
    {
        LB_TRACE_("do_done 已经清理过，无需重复执行");
        return;
    }

    m_done = true;

    LB_TRACE_("do_done 清理1：把阻塞线程唤醒");
    m_cv.notify_one(); // 由于只控制一个线程

    if (m_thread && m_thread->joinable())
    {
        LB_TRACE_("do_done 清理2：等待线程处理完剩余资源");
        m_thread->join();
    }
}

void AsyncLogging::waitDone() { do_done(); }

// 双缓冲关键代码
void AsyncLogging::append(const char* line, int len)
{
    // 下面为关键逻辑，采取双缓冲机制，如果缓存足够则push进去，否则将缓存转移到vector中待flush到磁盘
    // 虽然是这样说双缓冲，实际上不存在两个层级的缓存，第二个层级只是存储待push缓存的指针，并不会有拷贝
    // 为了减少重复申请内存的性能开销，又增加了备用内存nextBuffer，如果缓冲区需要新的内存，则直接使用它
    assert(!m_done);
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_curBuffer->avail() > len)
    { // 缓存足够
        m_curBuffer->append(line, len);
        return;
    }

    LB_TRACE_("AsyncLogging满了，通知线程消费");
    // 缓存满了，需要flush
    m_buffers.push_back(std::move(m_curBuffer)); // 转移指针所有权
    if (m_nextBuffer)
    { // 如果备用内存存在，则直接使用备用内存
        LB_TRACE_("备用内存启用成功");
        m_curBuffer = std::move(m_nextBuffer);
    }
    else
    { // 否则重新申请
        LB_TRACE_("备用内存启用失败，申请新的内存空间");
        m_curBuffer = elog::make_unique<Buffer>();
    }
    m_curBuffer->append(line, len);
    m_cv.notify_one(); // 通知消费
}

// 异步写入和内存复用
void AsyncLogging::thread_worker()
{
    try
    {
        // 不需要线程安全的写入，因本身使用它就会加锁
        LogFile   output(m_basename, m_rollSize, false);
        // 防止重复内存申请的内存
        BufferPtr newBuffer1 = elog::make_unique<Buffer>();
        BufferPtr newBuffer2 = elog::make_unique<Buffer>();
        newBuffer1->bzero();
        newBuffer2->bzero();
        // 用于帮助m_buffers写入磁盘的vector，每次通过swap将m_buffers的内容重置，目的是缩小临界区
        BufferVectorPtr buffersToWrite;
        buffersToWrite.reserve(16); // 提前预分配

        static bool once = false;

        while (!m_done)
        {
            LB_TRACE_("开始新一轮循环");
            {
                std::unique_lock<std::mutex> lock(m_mtx);
                LB_TRACE_("获取锁成功");
                if (m_buffers.empty())
                { // 等待flush_interval后，不管是否有任务，都把cur_buffer刷入buffers，然后再交换到buffer
                  // write进行写入，减小临界区
                    if (!once)
                    {
                        LB_TRACE_("后台工作线程初始化成功，准备唤醒等待任务");
                        once = true;
                        m_latch
                            .countDown(); // 只执行一次，外界会等待thread任务执行到这
                    }
                    m_cv.wait_for(lock, std::chrono::seconds(m_flushInterval));
                    LB_TRACE_("被唤醒 或超时{:d}s", m_flushInterval);
                }
                m_buffers.push_back(std::move(m_curBuffer));
                m_curBuffer = std::move(newBuffer1); // 换上新的内存
                buffersToWrite.swap(m_buffers);
                if (!m_nextBuffer)
                { // 顺便更新下nextBuffer的内存
                    LB_TRACE_("更新nextBuffer");
                    m_nextBuffer = std::move(newBuffer2);
                }
            }

            if (buffersToWrite.empty())
            {
                continue;
            }

            if (buffersToWrite.size() > 25)
            { // FIXME
              // 如果刷入的内存块超过25个，则丢弃到只剩下两个,后续可以加入告警通知异常
                LB_TRACE_("超出最高缓存数量，将丢弃只剩2个");
                char buf[256];
                snprintf(buf, sizeof buf,
                         "Dropped log messages at %s, %zd larger buffers\n",
                         Util::getCurDateTime(true), buffersToWrite.size() - 2);
                fputs(buf, stderr);
                output.append(buf, static_cast<int>(strlen(buf)));
                buffersToWrite.erase(buffersToWrite.begin() + 2,
                                     buffersToWrite.end());
            }

            // 开始写入
            LB_TRACE_("开始读取缓存进行写入");
            for (const auto& buffer : buffersToWrite)
            {
                output.append(buffer->data(), buffer->size());
            }
            if (buffersToWrite.size() > 2)
            {
                buffersToWrite.resize(
                    2); // 保留两个，可以进行资源的利用，即使没有2个，resize也会为你开两个空的
            }

            if (!newBuffer1)
            { // 由于前面必定存留两个，则可以直接利用
                LB_TRACE_("nweBuffer1更新");
                assert(!buffersToWrite.empty());
                newBuffer1 = std::move(buffersToWrite.back());
                buffersToWrite.pop_back();
                newBuffer1->reset(); // 记得重新设定内部读指针
            }

            if (!newBuffer2)
            { // 由于前面必定存留两个，则可以直接利用
                LB_TRACE_("newBuffer2更新");
                assert(!buffersToWrite.empty());
                newBuffer2 = std::move(buffersToWrite.back());
                buffersToWrite.pop_back();
                newBuffer2->reset();
            }

            buffersToWrite.clear();
            output.flush();
            LB_TRACE_("缓存刷新成功");
        }
        output.flush();
        LB_TRACE_("线程正常退出");
    }
    catch (std::exception const& e)
    {
        LB_TRACE_("线程非正常退出 error:{}", e.what());
        fprintf(stderr, "log thread exit: %s\n", e.what());
        m_thread.reset();
    }
}
* [使用教程](#使用教程)
    * [如何添加到项目中使用](#如何添加到项目中使用)
        * [CMake配置](#cmake配置)
        * [库的编译](#库的编译)
    * [如何使用库](#如何使用库)
        * [如何配置](#如何配置)
        * [精确控制输出等级](#精确控制输出等级)
        * [日志输出介绍](#日志输出介绍)
* [整体架构](#整体架构)

# 使用教程
想要从项目中引入该库，可以在cmake中简单的加入下面这几行代码，然后把my-logger链接到项目中即可。
```cmake
include(FetchContent)
FetchContent_Declare(
        elog4cpp
        GIT_REPOSITORY https://github.com/ACking-you/elog4cpp.git
        GIT_TAG v2.1
        GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(elog)
```

上面的cmake语句可能有些国人由于网络环境无法获取，我在gitee也建了个仓库供大家使用，对应的cmake如下：
```cmake
include(FetchContent)
FetchContent_Declare(
        my-logger
        GIT_REPOSITORY https://gitee.com/acking-you/my-logger.git
        GIT_TAG v1.6.2
        GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(my-logger)
```
## 除以上引入教程外，下面的内容均已无效(之后抽时间再更新使用文档)
## 如何添加到项目中使用

输入以下命令得到项目文件

```
git clone https://github.com/ACking-you/my-logger.git
```

项目目录如下：

- dependencies：整个项目的外部依赖，以源码形式存在（比如fmt库）。

<!---->

- lib：整个项目已经打包好的库文件，我打包的是动态库，如有静态库需要请自行编译。库的名称为logger和fmt。

<!---->

- 其余均为项目关键源码，就Logger.cpp和Logger.h两个。main.cc是项目运行测试代码。

### CMake配置

由于本人对 `cmake` 的 `install` 指令了解的并不深入，所以本库并不支持最后的 `make install`
，为了防止大伙编译库的长时间等待，我已经编译好了动态库版本，所以直接拿去用即可（当然不排除环境的不兼容，所以可能还是需要手动编译）。

**如果你clone本项目和你的项目为同级目录**，请在你的cmake文件中添加下面的代码，然后再链接这两个库便可得到本库的所有支持。

```cmake
include_directories(../my-logger/include)#指定项目相关的inclue目录

include_directories(../my-logger/dependencies/fmt/include)#指定项目所依赖的fmt库include目录

link_directories(../my-logger/lib)#指定项目依赖的lib目录（需要手动编译后放在这里
...

target_link_libraries(${CMAKE_PROJECT_NAME} logger fmt)#添加对应的链接库，注意使用多线程时请注意手动链接pthread
```

> 注意：如果你不需要使用fmt库中的高级功能，那么你可以不链接fmt库，如果需要使用其高级功能进行打印（比如支持打印std::
> map等标准库容器），那么请链接fmt库（当然这个库需要你自己编译）。

### 库的编译

下面为logger库的编译命令：

```
mkdir build-logger
cd build-logger
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
make
```

输入上面的命令后，会在 `build-logger`
文件夹中生成对应的动态库（不定义第二个变量时默认编译静态库）。建议把这个生成的动态库移动到这个项目 `lib`
目录中，方便在使用时只需添加之前的cmake代码。

下面为fmt库的编译命令（由于该库较大，编译时间可能较久，我的电脑用了将近十分钟）：

```
mkdir build-fmt
cd build-fmt
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
```

同样会在 `build-fmt` 目录生成对应的动态库，建议同样放入该项目lib目录中，注意 fmt 库的动态库是由多个文件构成，这多个文件都需要被放在一起。

## 如何使用库

### 如何配置

> 如下，为整个日志的所有配置项目，当然你也可以不进行配置，也有对应的默认值

```cpp
#include"LoggerImpl.h"

int main(){
Config::Set({
                    .print_flag = LstdFlags |
                                  lblog::LthreadId, //设置打印的内容，有日期、时间、文件名（长、短）、行号、线程id这些选项可选 （默认为LstdFlags包含Ldata | Ltime | Lshortname | Lline
.output_prefix = "my",          //设置输出日志的前缀名，默认为空
.output_basedir = "./log",     //设置日志输出文件夹位置，默认不添加该选项
.is_console = true  //设置是否输出到控制台，默认为true
});
}
```

注意：如果需要调用此配置函数进行配置，需要在所有打印日志宏调用之前，且同一个进程请不要使用两次配置（因为整个配置是一个单例，配置的更改在同一个进程中会是非线程安全的行为）。

>
update：更新了异步日志后，你只需指定需要输出的文件夹，然后日志库会自动进行文件输出，输出名称按照 `.日期-时间.用户名.进程号`
的样式，如果在日志输出超过1s的前提下，一个文件的大小大于20m，那么日志库会重新创建一个新的文件进行日志的输出。

### 精确控制输出等级

我们的库共提供以下五种宏来进行日志的打印：

- debug、info、warn、error、fatal

使用debug进行日志输出的，在release模式下将不再进行输出。

我们同样也可以通过手动定义对应的宏控制日志输出等级。

如下：

```cpp
#define LOG_LIMIT_WARN
#include "LoggerImpl.h"

int main(){
    debug("hhh"); //无效
    info("hhh");  //无效
    warn("hhh");  //有效
    error("hhh"); //有效
    fatal("hhh"); //有效
}
```

同理也可以通过 `LOG_LIMIT_ERROR` 控制至少是error等级才输出日志。

### 日志输出介绍

前面介绍了基本的日志的控制，接下来介绍打印输出的功能，以debug和info为例：

本日志的最终输出使用的是C++第三方库 fmt ，虽然在C++20中它已经入标准库了，但是距离我们真正的使用还需要一段时间，下面是简单输出方式，更多的输出方式可以查看fmt官方文档：

[fmt官方文档](https://fmt.dev/latest/index.html)

```cpp
#include"LoggerImpl.h"

int main(){
    debug("hello {}","world");
    info("world {}","hello");
}
```

> 注意：`{}`
> 是支持所有的C++标准库容器的，包括vector和string等等容器的直接打印，如 `debug("{}",vector<int>{1,3,32,432,432});`
> 是可行的。但不要忘了链接 fmt 库。

上面的默认输出效果如下：（控制台内是带颜色的，文件就没有颜色了）

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f0b7935159eb4915bbda6fc4dbfa6936~tplv-k3u1fbpfcp-zoom-1.image)

**增加回调支持**
> 如果你有统一的格式需求可以通过在最开始的Config中增加对应的回调函数，before对应的是基本格式打印之前的处理，end对应的是基本格式打印之后的处理。

将上面的代码做如下配置，并输出：
>
update：由于为了适应双缓冲异步输出的后台服务，对回调函数的接口进行了更改，下面的使用方式已经不再使用，具体使用方式请看 [test/main.cc](./tests/test_and_bench.cc)
中的 `test_multithreadAndProcess` 函数。

```cpp
#include"LoggerImpl.h"
using namespace lblog;
int main(){
    Config::Set({
        .before = [](FILE* f){ fmt::print(f,bg(fmt::color::blue),"我是before函数");},
        .after = [](FILE* f){fmt::print(f,bg(fmt::color::orange),"我是end函数");}
    });
    
    debug("hello {}","world");
    info("world {}","hello");
}
```

打印输出如下：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9b0ca1ccd57f4c46a4c5f8db41bf68a9~tplv-k3u1fbpfcp-watermark.image?)

# 整体架构

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/434f4d927a9049008de7570b30d55511~tplv-k3u1fbpfcp-watermark.image?)

> update：这个架构现在已经过时，由于加入了异步双缓冲，所以现在这个架构仅仅只是前端输出的架构。
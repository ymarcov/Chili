# Yam::HTTP Library

***A simple to use, yet efficient, modern C++ HTTP server library***

This is a library for developing HTTP servers with custom request processing. \
It uses asynchronous I/O operations, relying on *epoll* (so it is Linux-only), and reaches some very good numbers in terms of performance. At the same time, it's very easy to set up, as shown below.

It has a light footprint, and it employs modern C++ constructs, and compiles well under G++ 4.9.2 with `-std=c++14`.

### Feature Summary

- Asynchronous I/O
- Independent Read/Write throttling, both for the server as a whole and for individual connections
- Supports streaming
- Supports conditional message body fetching & rejection
- Supports cached responses for added efficiency
- Efficient, does not overload the CPU or memory when not required
- Lightweight, only depends on the C++ Standard Library and a small external static library for fast string formatting
- Uses modern C++ and is easy to use, and even to customize the code
- Public API documented with Doxygen
- Thoroughly tested with Google Test unit & integration tests, and also with Valgrind Memcheck

## Example Code (Hello World)

```c++
class HelloWorldChannel : public Channel {
    // Use constructors from Channel
    using Channel::Channel;

    // Process incoming requests
    Control Process(const Request&, Response& res) override {
        res.SetContent("<h1>Hello world!</h1>");
        res.SetField("Content-Type", "text/html");
        return SendResponse(Status::Ok);
    }
};

class HelloWorldChannelFactory : public ChannelFactory {
    std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override {
        return std::make_unique<HelloWorldChannel>(std::move(fs));
    }
};

int main() {
    auto endpoint = IPEndpoint({127, 0, 0, 1}, 3000);
    auto factory = std::make_shared<HelloWorldChannelFactory>();
    auto processingThreads = 1;
    HttpServer server(endpoint, factory, processingThreads);
    Log::Default()->SetLevel(Log::Level::Info);
    server.Start().wait();
}
```

## Getting Started
### Install Build Prerequisites
Assuming you're on Ubuntu/Debian,

```bash
$ sudo apt-get install cmake libcurl4-gnutls-dev libgtest-dev google-mock libunwind-dev
```
### Compile & Run

```bash
$ git clone https://github.com/ymarcov/http
$ cd http
$ git submodule update --init
$ cmake .
$ make hello_world
$ bin/hello_world
```

Then connect to `http://localhost:3000` from your browser.

## Documentation (Doxygen)
Documentation may be generated by running ```doxygen Doxyfile```. \
After that, it'll be available under `doc/html/index.html`.

## Performance
Running on Intel i7-4790 @ 3.6GHz with 8GB of memory, here are SIEGE results. In this test, it actually outperformed all of the other servers, with *nginx* coming in second. However, keep in mind that those other servers are bigger projects, and more configurable in terms of the protocols they support, etc. Also, I didn't try very hard to optimize them. I just installed them on my Debian Jessie machine and ran the same benchmark on their default home page.

```bash
$ bin/sandbox_echo 3000 4 0 & # Run sandbox server with 4 threads
$ siege -b -c20 -t5S http://localhost:<PORT>
```

#### Yam::HTTP

```
Transactions:		      194194 hits
Availability:		      100.00 %
Elapsed time:		        4.41 secs
Data transferred:	      115.56 MB
Response time:		        0.00 secs
Transaction rate:	    44034.92 trans/sec
Throughput:		       26.20 MB/sec
Concurrency:		       19.49
Successful transactions:      194194
Failed transactions:	           0
Longest transaction:	        0.01
Shortest transaction:	        0.00
```

#### nginx 1.6.2

```
Transactions:		      143034 hits
Availability:		      100.00 %
Elapsed time:		        4.48 secs
Data transferred:	       70.52 MB
Response time:		        0.00 secs
Transaction rate:	    31927.23 trans/sec
Throughput:		       15.74 MB/sec
Concurrency:		       19.61
Successful transactions:      143034
Failed transactions:	           0
Longest transaction:	        0.02
Shortest transaction:	        0.00
```

#### Lighttpd 1.4.35
```
Transactions:		       78138 hits
Availability:		      100.00 %
Elapsed time:		        4.05 secs
Data transferred:	      255.22 MB
Response time:		        0.00 secs
Transaction rate:	    19293.33 trans/sec
Throughput:		       63.02 MB/sec
Concurrency:		       19.76
Successful transactions:       78138
Failed transactions:	           0
Longest transaction:	        0.01
Shortest transaction:	        0.00
```

#### Apache 2.4.10

```
Transactions:		       67174 hits
Availability:		      100.00 %
Elapsed time:		        4.35 secs
Data transferred:	      219.41 MB
Response time:		        0.00 secs
Transaction rate:	    15442.30 trans/sec
Throughput:		       50.44 MB/sec
Concurrency:		       19.80
Successful transactions:       67174
Failed transactions:	           0
Longest transaction:	        0.01
Shortest transaction:	        0.00
```

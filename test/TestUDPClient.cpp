/**
 * TestClient.cpp - Client testing code
 * 2024-04-15
 * vika <https://github.com/hi-im-vika>
 */

#include <iostream>
#include <queue>
#include <csignal>
#include <numeric>

#include "../include/CUDPClient.hpp"

#define PING_TIMEOUT 1000
#define NET_DELAY 35

volatile sig_atomic_t stop;
volatile bool send_data = true;
volatile bool stop_main = false;

void catch_signal(int sig) {
//    stop = 1;
    stop_main = true;
}

void do_listen(CUDPClient *c, std::queue<std::string> *q) {
    std::vector<uint8_t> rx_buf;
    long rx_bytes;
    while (stop != 1) {
        rx_bytes = 0;
        rx_buf.clear();
//        spdlog::info("Listening");
        c->do_rx(rx_buf,rx_bytes);
        std::string temp = std::string(rx_buf.begin(),rx_buf.begin() + rx_bytes);
        // only add to rx queue if data is not empty
        if(!temp.empty()) q->emplace(temp);
        std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::milliseconds(1));
    }
}

void do_send(CUDPClient *c, std::queue<std::string> *q) {
    while (stop != 1) {
        for (; !q->empty(); q->pop()) {
//            spdlog::info("Sending");
            std::vector<uint8_t> tx_buf(q->front().begin(),q->front().end());
            c->do_tx(tx_buf);
        }
        std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::milliseconds(NET_DELAY));
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: client <host> <port>" << std::endl;
        return 1;
    }

    std::queue<std::string> tx_queue, rx_queue;
    std::chrono::steady_clock::time_point timeout_count;
    int time_since_start;

    std::vector<int> _rtimes;

    signal(SIGINT, catch_signal);
    CUDPClient c = CUDPClient();
    c.setup(argv[1], argv[2]);
    timeout_count = std::chrono::steady_clock::now();
    send_data = c.get_socket_status();
    // start listen thread
    std::thread thread_for_listening(do_listen, &c, &rx_queue);
    thread_for_listening.detach();

    // start send thread
    std::thread thread_for_sending(do_send, &c, &tx_queue);
    thread_for_sending.detach();
    while(!stop_main) {
        for (; !rx_queue.empty(); rx_queue.pop()) {

//            // acknowledge next data in queue
//            spdlog::info("New in RX queue with size: " + std::to_string(rx_queue.front().size()));
//            spdlog::info("Content: " + std::string(rx_queue.front().begin(), rx_queue.front().end()));
//            spdlog::info("Remaining in queue: " + std::to_string(rx_queue.size()));

            // reset timeout
            // placement of this may be a source of future bug
            timeout_count = std::chrono::steady_clock::now();
        }

        time_since_start = (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timeout_count).count();
        if (time_since_start > PING_TIMEOUT) {
            spdlog::warn("Server is gone...");
            send_data = false;
            do {
                c.setdn();
                c.setup(argv[1], argv[2]);
            } while (!c.get_socket_status());
            send_data = c.get_socket_status();
            timeout_count = std::chrono::steady_clock::now();
        }
        // send current time as payload
//        tx_queue.emplace(std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));
        tx_queue.emplace("A1 B1 C1 D1 E1 F1");
        spdlog::info("Last response time (ms): " + std::to_string(c.get_last_response_time()));
        if(_rtimes.size() < 1000) {
            _rtimes.push_back(c.get_last_response_time());
        } else {
            _rtimes.erase(_rtimes.begin());
            _rtimes.push_back(c.get_last_response_time());
        }
        spdlog::info("Avg response time (ms): {:f}", (float) (std::reduce(_rtimes.begin(), _rtimes.end()) / (float) _rtimes.size()));
        std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::milliseconds(NET_DELAY));
    }

    // tx EOT to stop
    spdlog::info("Stopping nicely");
    tx_queue.emplace("\4");

    // wait for send stop...
    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::milliseconds(100));
    stop = 1;
    if (thread_for_listening.joinable()) thread_for_listening.join();
    if (thread_for_sending.joinable()) thread_for_sending.join();
    spdlog::info("Goodbye");
    return 0;
}
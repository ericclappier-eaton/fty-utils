/*  ========================================================================
    Copyright (C) 2020 Eaton
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    ========================================================================
*/
#include "fty/event.h"
#include <catch2/catch.hpp>
#include <condition_variable>
#include <iostream>
#include <thread>

static int memCall   = 0;
static int lamCall   = 0;
static int scopeCall = 0;
static int statCall  = 0;

class Consumer
{
public:
    Consumer(fty::Event<int, std::string>& sig)
    {
        slot.connect(sig);
    }

    void slot1(int val, std::string sval)
    {
        ++memCall;
        CHECK((val == 42 || val == 112));
        CHECK((sval == "42" || sval == "112"));
    }

private:
    fty::Slot<int, std::string> slot = {&Consumer::slot1, this};
};

static void func(int val, std::string sval)
{
    ++statCall;
    CHECK((val == 42 || val == 112));
    CHECK((sval == "42" || sval == "112"));
}

TEST_CASE("Event")
{
    fty::Event<int, std::string> sig;

    Consumer cons(sig);

    fty::Slot<int, std::string> slot2([](int val, std::string sval) {
        ++lamCall;
        CHECK((val == 42 || val == 112));
        CHECK((sval == "42" || sval == "112"));
    });
    slot2.connect(sig);

    fty::Slot<int, std::string> slot4(&func);
    sig.connect(slot4);

    {
        fty::Slot<int, std::string> slot3([](int val, std::string sval) {
            ++scopeCall;
            CHECK(val == 42);
            CHECK(sval == "42");
        });
        sig.connect(slot3);
        sig(42, "42");
    }

    sig(112, "112");

    CHECK(memCall == 2);
    CHECK(lamCall == 2);
    CHECK(statCall == 2);
    CHECK(scopeCall == 1);

    auto sig1 = std::move(sig);
    sig1(112, "112");
    CHECK(memCall == 3);
    CHECK(lamCall == 3);
    CHECK(statCall == 3);
    CHECK(scopeCall == 1);
}

TEST_CASE("Event thread")
{
    fty::Event<int&, std::string&> sig;

    std::condition_variable var;
    std::mutex              mutex;
    bool                    fired = false;
    bool                    ready = false;
    int                     val = 0;
    std::string             sval {""};

    std::thread th2([&]() {
        fty::Slot<int&, std::string&> slot([&](int& iniVal, std::string& insVal) {
            iniVal = 42;
            insVal = "42";
        });
        sig.connect(slot);

        {
            std::lock_guard<std::mutex> lk(mutex);
            ready = true;
        }
        var.notify_one();

        std::unique_lock<std::mutex> lk(mutex);
        var.wait(lk, [&] {
            return fired;
        });
    });

    std::thread th1([&]() {
        {
            std::unique_lock<std::mutex> lk(mutex);
            var.wait(lk, [&] {
                return ready;
            });

            sig(val, sval);
            fired = true;
        }
        var.notify_one();
    });

    th2.join();
    th1.join();

    CHECK(42 == val);
    CHECK("42" == sval);
}

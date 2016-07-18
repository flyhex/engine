/**
 * @file
 */

#include "core/tests/AbstractTest.h"
#include "DatabaseModels.h"
#include "persistence/ConnectionPool.h"
#include "config.h"

namespace backend {

class DatabaseModelTest: public core::AbstractTest {
	using Super = core::AbstractTest;
public:
	void SetUp() override {
		Super::SetUp();
		::persistence::ConnectionPool::get().init();
	}

	void TearDown() override {
		Super::TearDown();
		::persistence::ConnectionPool::get().shutdown();
	}
};

TEST_F(DatabaseModelTest, testCreate) {
	persistence::UserStore u;
	ASSERT_TRUE(u.createTable()) << "Could not create table";
}

TEST_F(DatabaseModelTest, testWrite) {
	const std::string email = "a@b.c.d";
	const std::string password = "secret";
	persistence::UserStore::truncate();
	const ::persistence::Timestamp ts = ::persistence::Timestamp::now();
	persistence::UserStore u(&email, &password, &ts);
	ASSERT_EQ(0, u.userid());
	u.insert(email, password, ts);
	ASSERT_NE(0, u.userid());
}

}

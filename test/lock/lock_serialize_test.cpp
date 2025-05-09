// #include <gtest/gtest.h>

// extern "C" {
// #include <lock/lock_internal.h>
// #include "utils/logger.h"
// #include "utils/utils.h"
// }

// TEST(serialize_lock, try_serialize_nodeid)
// {
// 	init_logger(stdout);

// 	struct node_id src = { .host = "127.0.0.1", .port = 7777 }, dest;

// 	char buff[NODEID_SIZE];

// 	serialize_nodeid(buff, &src);
// 	unserialize_nodeid(&dest, buff);

// 	log_info("src = %s:%d", src.host, src.port);
// 	log_info("dest = %s:%d", dest.host, dest.port);

// 	ASSERT_EQ(node_equal(&src, &dest), true);
// }

// TEST(serialize_lock, try_serialize_request)
// {
// 	init_logger(stdout);

// 	struct request src = {
//         .mode = WRITE,
//         .who = {
//             .host = "127.0.0.1",
//             .port = 7777,
//         }
//     }, dest;

// 	char buff[REQUEST_SIZE];

// 	serialize_request(buff, &src);
// 	unserialize_request(&dest, buff);

// 	log_info("request src %s:%d in %s", src.who.host, src.who.port,
// 		 src.mode == WRITE ? "WRITE" : "READ");

// 	log_info("request dest %s:%d in %s", dest.who.host, dest.who.port,
// 		 dest.mode == WRITE ? "WRITE" : "READ");

// 	ASSERT_EQ(node_equal(&src.who, &dest.who), true);
// 	ASSERT_EQ(src.mode, dest.mode);
// }

// TEST(serialize_lock, try_serialize_delegate)
// {
// 	init_logger(stdout);

// 	struct delegate_message src = {
//         .message_type = DELEGATE,
// 		.sender = {
// 			.host = "127.0.0.1",
//             .port = 7777,
// 		},
// 		.delegate = {
// 			.host = "127.0.0.2",
//             .port = 8888,
// 		}
//     }, dest;

// 	char buff[DELEGATE_MESSAGE_SIZE];

// 	serialize_delegate_message(buff, &src);
// 	dest = unserialize_delegate_message(buff);

// 	log_info("delegate src %s:%d -> %s:%d", src.sender.host,
// 		 src.sender.port, src.delegate.host, src.delegate.port);
// 	log_info("delegate dest %s:%d -> %s:%d", dest.sender.host,
// 		 dest.sender.port, dest.delegate.host, dest.delegate.port);

// 	ASSERT_EQ(node_equal(&src.sender, &dest.sender), true);
// 	ASSERT_EQ(node_equal(&src.delegate, &dest.delegate), true);
// 	ASSERT_EQ(src.message_type, dest.message_type);
// }
CREATE DATABASE chatting;

-- Create Authentication Table
CREATE TABLE chatting.Authentication (
    uuid INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    email VARCHAR(100) UNIQUE
 );

 -- Create UserProfile Table
 CREATE TABLE chatting.UserProfile (
    uuid INT PRIMARY KEY,
    avatar VARCHAR(255),
    nickname VARCHAR(50),
    description TEXT,
    sex BOOL,
    FOREIGN KEY (uuid) REFERENCES Authentication(uuid) ON DELETE CASCADE
 );

-- Create Friend Request Table
CREATE TABLE chatting.FriendRequest(
    id INT AUTO_INCREMENT PRIMARY KEY,
	src_uuid INT NOT NULL,
    dst_uuid INT NOT NULL,
    nickname VARCHAR(255),
    message VARCHAR(255),
    status BOOL,	-- request status
    FOREIGN KEY (src_uuid) REFERENCES Authentication(uuid) ON DELETE CASCADE,
    FOREIGN KEY (dst_uuid) REFERENCES Authentication(uuid) ON DELETE CASCADE
);

-- Create Auth Friend Table
CREATE TABLE chatting.AuthFriend(
    id INT AUTO_INCREMENT PRIMARY KEY,
	self_uuid INT NOT NULL,
    friend_uuid INT NOT NULL,
	alternative_name VARCHAR(255),	--
    FOREIGN KEY (self_uuid ) REFERENCES Authentication(uuid) ON DELETE CASCADE,
    FOREIGN KEY ( friend_uuid ) REFERENCES Authentication(uuid) ON DELETE CASCADE
);

CREATE UNIQUE INDEX idx_self_friend ON chatting.AuthFriend(self_uuid ASC, friend_uuid ASC) USING BTREE;

-- Create Global Thread Info(related to Group and Private chat)
CREATE TABLE chatting.GlobalThreadIndexTable(
	id  BIGINT  UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '',
    type ENUM('PRIVATE', 'GROUP') NOT NULL,
	created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id)
);

-- Create private Chat(which includes thread id for this private chat and two user uuid!)
CREATE TABLE chatting.PrivateChat(
	thread_id  BIGINT  UNSIGNED NOT NULL COMMENT 'refer to chatting.GlobalThreadIndexTable.id',
    user1_uuid  BIGINT  UNSIGNED NOT NULL COMMENT 'chatting.Authentication.uuid',
    user2_uuid  BIGINT  UNSIGNED NOT NULL COMMENT 'chatting.Authentication.uuid',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (thread_id),
    UNIQUE KEY unique_thread (user1_uuid, user2_uuid),

    KEY search_user1 (user1_uuid, thread_id),
    KEY search_user2 (user2_uuid, thread_id)
);

-- Create Group Chat Thread Table
Create TABLE chatting.GroupThreadIndexTable(
    thread_id BIGINT UNSIGNED NOT NULL COMMENT 'refer to chatting.GlobalThreadIndexTable.id',
    name VARCHAR(255) DEFAULT NULL COMMENT 'the name of the group chat',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (thread_id)
);

-- Create Group Member Table
Create TABLE chatting.GroupMember(
    thread_id BIGINT UNSIGNED NOT NULL COMMENT 'refer to chatting.GroupThreadIndexTable. thread_id',
    user_uuid BIGINT UNSIGNED NOT NULL COMMENT 'chatting.Authentication.uuid',
    user_role TINYINT NOT NULL DEFAULT 0 COMMENT '0=user, 1=admin, 2=creator',
    joined_date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (thread_id, user_uuid),
     KEY search_user_threads(user_uuid)
);

-- Create Chatting Messages' History recored
CREATE TABLE chatting.ChatMsgHistoryBank(
	message_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'for server or client to recored their unique message',
    thread_id BIGINT UNSIGNED NOT NULL COMMENT 'refer to chatting.GlobalThreadIndexTable.id',
    message_status TINYINT NOT NULL DEFAULT 0 COMMENT '0=unread, 1=read, 2=revoke',
    message_sender BIGINT UNSIGNED NOT NULL COMMENT 'The sender of this message, refering to chatting.Authentication.uuid',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE current_timestamp,
    message_content TEXT NOT NULL,
    primary key (message_id),
    KEY search_thread_created  (thread_id, created_at),
    KEY search_thread_message (thread_id, message_id)
);
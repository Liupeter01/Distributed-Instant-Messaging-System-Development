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

INSERT INTO chatting.Authentication(username, password, email) VALUES ("test_user_a", "1#Ludhyno^&NkB5l", "email");
INSERT INTO chatting.Authentication(username, password, email) VALUES ("test_user_b", "*x%n$c8sB#8sBRG!", "email");
INSERT INTO chatting.Authentication(username, password, email) VALUES ("test_user_c", "W3YsH7BoOBi&s7EY", "email");
INSERT INTO chatting.UserProfile(uuid, avatar, nickname, description, sex) VALUES (1, "1.png", "c++", "hello c++", 0);
INSERT INTO chatting.UserProfile(uuid ,avatar, nickname, description, sex) VALUES (2, "2.png", "python", "hello python", 1);
INSERT INTO chatting.UserProfile(uuid ,avatar, nickname, description, sex) VALUES (3, "3.png", "nodejs", "hello nodejs", 1);
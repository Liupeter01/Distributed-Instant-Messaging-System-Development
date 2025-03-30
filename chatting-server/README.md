# Chatting-server

## 0x00 Description

1. User Login`(SERVICE_LOGINSERVER)`

2. ~~User Logout`SERVICE_LOGOUTSERVER`~~

3. User Search For Peers`(SERVICE_SEARCHUSERNAME)`

4. User Who Initiated Friend Request `(SERVICE_FRIENDREQUESTSENDER)`

5. User Who Received Friend Request`(SERVICE_FRIENDREQUESTCONFIRM)`

## 0x02 Requirements

### Basic Infrastructures

**It's strongly suggested to use docker to build up those services ^_^**

**If you intended to pass a host directory, please use absolute path.**

1. Redis Memory Database

   Create a local volume on host machine and editing configration files. **Please don't forget to change your password.**

   ```bash
   #if you are using windows, please download WSL2
   mkdir -p /path/to/redis/{conf,data} 
   cat > /path/to/redis/conf/redis.conf <<EOF
   # bind 192.168.1.100 10.0.0.1     # listens on two specific IPv4 addresses
   # bind 127.0.0.1 ::1              # listens on loopback IPv4 and IPv6
   # bind * -::*                     # like the default, all available interfaces
   # bind 127.0.0.1 -::1
   protected-mode no
   port 6379
   tcp-backlog 511
   timeout 0
   tcp-keepalive 300
   daemonize no
   pidfile /var/run/redis_6379.pid
   loglevel notice
   logfile ""
   databases 16
   always-show-logo no
   set-proc-title yes
   proc-title-template "{title} {listen-addr} {server-mode}"
   locale-collate ""
   stop-writes-on-bgsave-error yes
   rdbcompression yes
   rdbchecksum yes
   dbfilename dump.rdb
   rdb-del-sync-files no
   dir ./
   #---------------------------password--------------------------------------------
   requirepass 123456
   #---------------------------------------------------------------------------------
   replica-serve-stale-data yes
   replica-read-only yes
   repl-diskless-sync yes
   repl-diskless-sync-delay 5
   repl-diskless-sync-max-replicas 0
   repl-diskless-load disabled
   repl-disable-tcp-nodelay no
   replica-priority 100
   acllog-max-len 128
   lazyfree-lazy-eviction no
   lazyfree-lazy-expire no
   lazyfree-lazy-server-del no
   replica-lazy-flush no
   lazyfree-lazy-user-del no
   lazyfree-lazy-user-flush no
   oom-score-adj no
   oom-score-adj-values 0 200 800
   disable-thp yes
   appendonly no
   appendfilename "appendonly.aof"
   appenddirname "appendonlydir"
   appendfsync everysec
   no-appendfsync-on-rewrite no
   auto-aof-rewrite-percentage 100
   auto-aof-rewrite-min-size 64mb
   aof-load-truncated yes
   aof-use-rdb-preamble yes
   aof-timestamp-enabled no
   slowlog-log-slower-than 10000
   slowlog-max-len 128
   latency-monitor-threshold 0
   notify-keyspace-events ""
   hash-max-listpack-entries 512
   hash-max-listpack-value 64
   list-max-listpack-size -2
   list-compress-depth 0
   set-max-intset-entries 512
   set-max-listpack-entries 128
   set-max-listpack-value 64
   zset-max-listpack-entries 128
   zset-max-listpack-value 64
   hll-sparse-max-bytes 3000
   stream-node-max-bytes 4096
   stream-node-max-entries 100
   activerehashing yes
   client-output-buffer-limit normal 0 0 0
   client-output-buffer-limit replica 256mb 64mb 60
   client-output-buffer-limit pubsub 32mb 8mb 60
   hz 10
   dynamic-hz yes
   aof-rewrite-incremental-fsync yes
   rdb-save-incremental-fsync yes
   jemalloc-bg-thread yes
   EOF
   ```

   Creating a `Redis` container and execute following commands.

   ```bash
   docker pull redis:7.2.4  #Pull the official docker image from Docker hub
   docker run \
       --restart always \
       -p 16379:6379 --name redis \
       --privileged=true \
       -v /path/to/redis/conf/redis.conf:/etc/redis/redis.conf \
       -v /path/to/redis/data:/data:rw \
       -d redis:7.2.4 redis-server /etc/redis/redis.conf \
       --appendonly yes
   ```

   Entering `Redis` container and access to command line `redis-cli`.

   ```bash
   docker exec -it redis bash  #entering redis
   redis-cli            #login redis db
   ```

2. MySQL Database

   Create a local volume on host machine and editing configration files. **Please don't forget to change your password.**

   ```bash
   #if you are using windows, please download WSL2
   mkdir -p /path/to/mysql/{conf,data} 
   touch /path/to/mysql/conf/my.cnf #create
   cat > /path/to/redis/conf/redis.conf <<EOF
   [mysqld]
   default-authentication-plugin=mysql_native_password
   skip-host-cache
   skip-name-resolve
   datadir=/var/lib/mysql
   socket=/var/run/mysqld/mysqld.sock
   secure-file-priv=/var/lib/mysql-files
   user=mysql
   character-set-server=utf8
   max_connections=200
   max_connect_errors=10
   pid-file=/var/run/mysqld/mysqld.pid
   [client]
   socket=/var/run/mysqld/mysqld.sock
   default-character-set=utf8
   !includedir /etc/mysql/conf.d/
   EOF
   ```

   Creating a `MySQL` container and execute following commands.

   ```bash
   docker pull mysql:8.0  #Pull the official docker image from Docker hub
   docker run --restart=on-failure:3 -d \
       -v /path/to/mysql/conf:/etc/mysql/conf.d \
       -v /path/to/mysql/data:/var/lib/mysql \
       -e MYSQL_ROOT_PASSWORD="your_password" \
       -p 3307:3306 --name "your_container_name" \
       mysql:8.0
   ```

   Entering `MySQL` container and access to `mysql` command line.

   ```bash
   docker exec -it "your_container_name" bash  #entering mysql
   mysql -uroot -p"your_password"                #login mysql db ( -u: root by default, -p password)
   ```

   Initialise `MySQL` database with following `SQL` commands to create DB and table schemas.

   ```sql
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
       status BOOL, -- request status
       FOREIGN KEY (src_uuid) REFERENCES Authentication(uuid) ON DELETE CASCADE,
       FOREIGN KEY (dst_uuid) REFERENCES Authentication(uuid) ON DELETE CASCADE
   );
   ```

### Servers' Configurations

```ini
[BalanceService]
host=127.0.0.1
port=59900
[gRPCServer]
server_name=ChattingServer0
host=127.0.0.1
port=64400
[ChattingServer]
port=60000
send_queue_size=1000
[Redis]
host=127.0.0.1
port=16379
password=123456
[MySQL]
username=root
password=123456
database=chatting
host=localhost
port=3307
#timeoutsetting(s) for heart pulse
timeout=60
```

## 0x03 Developer Quick Start

### Platform Support

Windows, Linux, MacOS(Intel & Apple Silicon M)

### Download Chatting-server

```bash
git clone https://github.com/Liupeter01/chatting-server --depth 1
```

### Compile Chattingserver

grpc-1.50.2 will be downloaded automatically, and we will use boringssl instead of openssl

**For Windows users, fetch content will download all of those for you**

1. For Linux/Windows

   ```bash
   cd FullStackChattingDemo/chatting-server
   git submodule update --init
   cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel [x]
   ```

2. For MacOS

   ```bash
   cd FullStackChattingDemo/chatting-server
   git submodule update --init
   cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_INCLUDE_PATH=/usr/local/include
   cmake --build build --parallel [x]
   ```

### How to Execute

1. Activate Redis and MySQL service

   **IMPORTANT: you have to start those services first!!**

2. **Start to execute `balance-server` first**

3. Execute Servers' program

   ```bash
   ./chatting-server/build/ChattingServer
   ```

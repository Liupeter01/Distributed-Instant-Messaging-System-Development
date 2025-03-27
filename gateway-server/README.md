# Gateway-server

## 0x00 Description

All services are using HTTP short connections, users are going to create a POST method to the gateway-server and gateway-server is going to respond to the client requests accordingly.

1. `/get_verification`

   User sends a email address to gateway-server and request to get a Email verification code(CPATCHA) request to server. server using **gRPC** protocol to communicate with NodeJS server(`captcha-server`) and create an unique **uuid** for the user. The **uuid** is going to store in a **Redis** memory database with a timeout setting, user should register the new account within the valid time or request for a new one instead.

2. `/post_registration`

   After request for a **valid CPATCHA**, user could trigger registration confirm button to post registration request to the server. Server will whether this user's identity is collision with any other user inside the system, if no collision found the info will be stored inside database. ~~however, SQL injection protection mechanism is still not available yet!~~

3. `/check_accountexists`

   After account registration, when user demands to change his/her password, we have to verifiy the account existance.

4. `/reset_password`

   After executing `/check_accountexists` process, then user could enter his/her new password info, and client terminal could send the new password info to the the server. server will do the similiar process in `/post_registration` and alter the existing data inside the database.

5. `/trylogin_server`

   please be careful, `trylogin_server` **could not login into** the real server directly. **It's a server relay!**

   The identification is similiar to `/check_accountexists` authenication process. The `gateway-server` will communicate with `balance-server` for the address of `chatting-server` by using **gRPC**, and `chatting-server` will do load-balancing and return the lowest load server info back. However, The user connection status **will not** maintained and managed by `gateway-server` and `gateway-server` doesn't care about this either, client will receive the real address of `chatting-server` and connecting to it by itself. ~~however, SQL injection protection mechanism is still not available yet!~~

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
#config.ini
[GateServer]
port = 8080
[VerificationServer]
host=127.0.0.1
port = 65500
[MySQL]
username=root
password=123456
database=chatting
host=localhost
port=3307
#timeoutsetting(s) for heart pulse
timeout=60 
[Redis]
host=127.0.0.1
port=16379
password=123456
[BalanceService]
host=127.0.0.1
port=59900
```

## 0x03 Developer Quick Start

### Platform Support

Windows, Linux, MacOS(Intel & Apple Silicon M)

### Compile From Source Code

grpc-1.50.2 will be downloaded automatically, and we will use boringssl instead of openssl

**For Windows users, fetch content will download all of those for you**

1. Download From Github

   ```bash
   git clone https://github.com/Liupeter01/gateway-server --depth 1
   ```

2. Build From Source

   ```bash
   ###########################For Linux/Windows User################################
   cd FullStackChattingDemo/gateway-server
   git submodule update --init
   cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel [x]
   
   ###################################For MacOS###################################
   cd FullStackChattingDemo/gateway-server
   git submodule update --init
   cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_INCLUDE_PATH=/usr/local/include
   cmake --build build --parallel [x]
   ```

3. Execute Server From Executable File

   **Please Activate Redis and MySQL service First!**

   ```bash
   ./build/GatewayServer
   ```

### Docker

1. Download From GitHub

   ```bash
   git clone https://github.com/Liupeter01/gateway-server --depth 1
   ```

2. Execute Docker Build Command

   ```b
   docker build -t gateway-server-container .
   ```

3. Execute Docker Run To Start Server

   ```bash
   docker run -P -v /path/to/gateway/conf/config.ini:/app/config.ini gateway-server-container ./build/GatewayServer
   ```

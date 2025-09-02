DELETE FROM chatting.FriendRequest WHERE (id >= 0);
DELETE FROM chatting.AuthFriend WHERE (id >= 0);
DELETE FROM chatting.PrivateChat WHERE (thread_id >= 0);
DELETE FROM chatting.GlobalThreadIndexTable WHERE (id >= 0);
DELETE FROM chatting.ChatMsgHistoryBank WHERE (message_id >= 0);
CREATE TABLE chatMessages (
			id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
			senderName TEXT,
			channelType TEXT,
			channelName TEXT,
			messageText TEXT,
			creationTime TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
		);
CREATE TABLE sqlite_sequence(name,seq);
CREATE TABLE gameRoomPlayers (
			id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
			roomID TEXT,
			accountID TEXT
		);
CREATE TABLE gameRooms (
			id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
			roomID TEXT,
			hostAccountID TEXT,
			description TEXT NOT NULL DEFAULT '',
			status INTEGER NOT NULL DEFAULT 0,
			playerLimit INTEGER NOT NULL,
			creationTime TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
		, mods TEXT NOT NULL DEFAULT '{}', version TEXT NOT NULL DEFAULT '');
CREATE TABLE accounts (
			id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
			accountID TEXT,
			displayName TEXT,
			online INTEGER NOT NULL,
			lastLoginTime TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,
			creationTime TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
		);
CREATE TABLE accountCookies (
			id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
			accountID TEXT,
			cookieUUID TEXT,
			creationTime TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
		);
CREATE TABLE gameRoomInvites (
			id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
			roomID TEXT,
			accountID TEXT
		);
CREATE TABLE sqlite_stat1(tbl,idx,stat);

CREATE INDEX idx_accounts_online ON accounts(online);
CREATE INDEX idx_accounts_accountID ON accounts(accountID);
CREATE INDEX idx_accounts_displayName ON accounts(displayName COLLATE NOCASE);

CREATE INDEX idx_gameRooms_roomID ON gameRooms(roomID);
CREATE INDEX idx_gameRooms_status ON gameRooms(status);

CREATE INDEX idx_chatMessages_creationTime ON chatMessages(creationTime);
CREATE INDEX idx_chatMessages_channelTypeName ON chatMessages(channelType, channelName);

CREATE INDEX idx_gameRoomPlayers_accountID ON gameRoomPlayers(accountID);
CREATE INDEX idx_gameRoomPlayers_roomID ON gameRoomPlayers(roomID);

CREATE INDEX idx_accountCookies_accountID ON accountCookies(accountID);

CREATE INDEX idx_gameRoomInvites_accountID ON gameRoomInvites(accountID);

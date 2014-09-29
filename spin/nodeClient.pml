
inline MutexLock(v)
{
	atomic { assert(v != _pid + 1); v == 0; v = _pid + 1; }
}
inline MutexUnlock(v)
{
	d_step { assert(v != 0); v = 0; }
}

byte domainMutex;

#define MaxNumClients 2

mtype  {
	NotAccepted,
	WaitingForApplication,
	ConnectingToMaster,
	SettingUpApplication,
	Servicing,
	Closed
};

typedef Client
{
	byte mutex;
	bit clientExists;
	bit callsClientClosed;
	chan acceptClientInnerDone = [0] of { bit };
	mtype state = NotAccepted;
};

Client clients[MaxNumClients];


inline ClientClosed(clientId)
{
	if
	:: clients[clientId].callsClientClosed ->
		clients[clientId].callsClientClosed = 0;
		clients[clientId].clientExists = 0;
		MutexLock(domainMutex);
		MutexUnlock(domainMutex);
	:: else -> skip;
	fi
}

proctype AcceptClient(byte clientId)
{
	MutexLock(domainMutex);

	clients[clientId].callsClientClosed = 1;
	clients[clientId].clientExists = 1;
	
	run AcceptClientInner(clientId);

	clients[clientId].acceptClientInnerDone ? 0;


	MutexUnlock(domainMutex);
}

proctype AcceptClientInner(byte clientId)
{
	MutexLock(clients[clientId].mutex);

	assert(clients[clientId].state == NotAccepted);

	clients[clientId].state = WaitingForApplication;

	run ClientDispatchApp(clientId);

	MutexUnlock(clients[clientId].mutex);
	clients[clientId].acceptClientInnerDone ! 0;
}

proctype ClientReject(byte clientId)
{
	MutexLock(clients[clientId].mutex);
	
	assert(clients[clientId].state == NotAccepted ||
		clients[clientId].state == WaitingForApplication);

	clients[clientId].state = Closed;

	/* TODO: onConnectionRejected */
	ClientClosed(clientId);

	MutexUnlock(clients[clientId].mutex);
}

proctype ClientDispatchApp(byte clientId)
{
	/* createFunc might fail... */
	if
	:: skip -> 
		run ClientReject(clientId);
		goto done;
	:: skip -> skip;
	fi

	MutexLock(clients[clientId].mutex);
	if 
	:: clients[clientId].state == Closed ->
		MutexUnlock(clients[clientId].mutex);
	:: else ->
		clients[clientId].state = WaitingForApplication;
		run ConnectToMaster(clientId);
	fi

done:
	skip;
}

proctype ConnectToMaster(byte clientId)
{
	/* mutex is already locked by ClientDispatchApp */
	assert(clients[clientId].state == WaitingForApplication);
	
	clients[clientId].state = ConnectingToMaster;

	MutexUnlock(clients[clientId].mutex);

	MutexLock(clients[clientId].mutex);
	if
	:: skip -> /* Conncetion might fail... */
		error:
		clients[clientId].state = Closed;

		ClientClosed(clientId);
		MutexUnlock(clients[clientId].mutex);
		goto done;

	:: clients[clientId].state == Closed ->
		MutexUnlock(clients[clientId].mutex);
		goto done;

	:: clients[clientId].state != Closed ->
		skip;
	fi

	assert(clients[clientId].state == ConnectingToMaster);
	
	clients[clientId].state = SettingUpApplication;

	/* Creating sockets might fail... */
	if
	:: skip -> goto error;
	:: skip -> skip;
	fi

	clients[clientId].state = Servicing;
	MutexUnlock(clients[clientId].mutex);

done:
	skip;
}

init
{
	run AcceptClient(0);
	run AcceptClient(1);
	timeout;
	byte i;
	for (i : 0 .. MaxNumClients - 1) {
		assert(clients[i].state == Closed || clients[i].state == Servicing);
		assert(clients[i].state != Closed || !clients[i].clientExists);
		assert(clients[i].state == Closed || clients[i].clientExists);
	}
}

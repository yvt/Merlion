bit acceptsClient;
bit clientCreated;

bit addPrecedes;

proctype AddClient()
{
	addPrecedes = false;

	if
	:: acceptsClient -> clientCreated = true
	:: !acceptsClient -> clientCreated = false
	fi

}

proctype DenyAddClient()
{
	addPrecedes = true;

	acceptsClient = false;
}

init
{
	acceptsClient = true;
	clientCreated = false;

	run AddClient();
	run DenyAddClient();

	timeout;
	assert((addPrecedes && clientCreated) ||
		(!addPrecedes && !clientCreated));
}
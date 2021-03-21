# Interacting with Redis

## Install and Run as a Service

### Will not work on systemd

```bash
./utils/install_server.sh
```

## Set up Pulishing all Keyspace Events

```bash
redis-cli config set notify-keyspace-events KEA
```

## Subscribe to all Keyspace Events

```bash
redis-cli psubscribe '__key*__:*'
```

## Connecting to a Redis Server in the Host Namespace from a container

```bash
redis-cli -h <docker0 ip> -p 6379
```

## Connection to a Redis Server running in another container

### Deploy the NF container

```bash
sudo docker run -it --rm --name <NF name> --link <redis container name>:redis -d redis
```

### In the NF container

```bash
redis-cli -h redis
```

# iblock

iblock is an inetd program adding the client IP to a Packet Filter table.

It is meant to be used to block scanner connecting on unused ports.


# How to use

Start inetd service with this in `/etc/inetd.conf`:

```
666 stream tcp nowait root /usr/local/bin/iblock iblock
```

Use this in `/etc/pf.conf`, choose which ports will trigger the ban from the variable:

```
# services triggering a block
blocking_tcp="{ 3306 5432 3389 27019 }"

table <blocked> persist

pass in quick on egress proto tcp to port $blocking_tcp rdr-to 127.0.0.1 port 666
block in quick from <blocked>
```

Done! You can see IP banned using `pfctl -t blocked -T show` and iBlock will log blocking too.


# TODO

- make install doing something
- A proper man page
- Support IPv6
- make it work with doas
- pf table as a parameter

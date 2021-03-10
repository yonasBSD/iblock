# iblock

iblock is an inetd program adding the client IP to a Packet Filter table.

It is meant to be used to block scanner connecting on unused ports.


# How to use

Start inetd service with this in `/etc/inetd.conf`:

```
666 stream tcp nowait root /usr/local/bin/iblock iblock
666 stream tcp6 nowait root /usr/local/bin/iblock iblock
```

Use this in `/etc/pf.conf`, choose which ports will trigger the ban from the variable:

```
# services triggering a block
blocking_tcp="{ 21 23 53 111 135 137:139 445 1433 25565 5432 3389 3306 27019 }"

table <blocked> persist

block in quick from <blocked> label iblock
pass in quick on egress inet proto tcp to port $blocking_tcp rdr-to 127.0.0.1 port 666
pass in quick on egress inet6 proto tcp to port $blocking_tcp rdr-to ::1 port 666
```

Done! You can see IP banned using `pfctl -t blocked -T show` and iBlock will log blocking too.

In the example I added a label to the block rule, you can use `pfctl -s labels` to view statistics from this rule, [see documentation for column meaning](https://man.openbsd.org/pfctl#s~8).


# TODO

- make install doing something
- A proper man page
- Support IPv6
- make it work with doas
- pf table as a parameter

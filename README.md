# iblock

iblock is an inetd program adding the client IP to a Packet Filter table.

It is meant to be used to block scanner connecting on unused ports.

Upon connection, the IP is added to a PF table and all established connections with this IP are killed.  You need to use a PF bloking rule using the table.

# How to use

## Add a dedicated user

```
useradd -s /sbin/nologin _iblock
```

## Configure doas

Add in `/etc/doas.conf`:

```
permit nopass _iblock cmd /sbin/pfctl
```

## Configure inetd

Start inetd service with this in `/etc/inetd.conf`:

```
666 stream tcp nowait _iblock /usr/local/sbin/iblock iblock
666 stream tcp6 nowait _iblock /usr/local/sbin/iblock iblock
```

You can change the PF table by adding it as a parameter like this:

In this example, the parameter `blocklist` will add IPs to the `blocklist` PF table.

```
666 stream tcp nowait _iblock /usr/local/sbin/iblock iblock blocklist
666 stream tcp6 nowait _iblock /usr/local/sbin/iblock iblock blocklist
```

Default is "iblocked" table.

## Configure packet filter

Use this in `/etc/pf.conf`, choose which ports will trigger the ban from the variable:

```
# services triggering a block
blocking_tcp="{ 21 23 53 111 135 137:139 445 1433 25565 5432 3389 3306 27019 }"

table <blocked> persist

block in quick from <blocked> label iblock
pass in quick on egress inet proto tcp to port $blocking_tcp rdr-to 127.0.0.1 port 666
pass in quick on egress inet6 proto tcp to port $blocking_tcp rdr-to ::1 port 666
```

Don't forget to reload the rules with `pfctl -f /etc/pf.conf`.

# Get some statistics

Done! You can see IP banned using `pfctl -t blocked -T show` and iBlock will send blocked addresses to syslog.

In the example I added a label to the block rule, you can use `pfctl -s labels` to view statistics from this rule, [see documentation for column meaning](https://man.openbsd.org/pfctl#s~8).


# TODO

- A proper man page

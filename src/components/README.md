Stateful component hierarchy

```
  server_list
   |
   |__*server
       |
       |__buffer
       |   |
       |   |__*buffer_line
       |
       |__channel
       |
       |__channel_list
       |   |
       |   |__*channel
       |       |
       |       |__buffer
       |       |   |
       |       |   |__*buffer_line
       |       |
       |       |__input
       |       |   |
       |       |   |__*input_line
       |       |
       |       |__mode
       |       |__mode_str
       |       |
       |       |_user_list
       |         |
       |         |__*user
       |             |
       |             |__mode
       |
       |__connection
       |
       |__ircv3_caps
       |   |
       |   |__*ircv3_cap
       |
       |__mode
       |__mode_str
       |__mode_cfg
       |
       |__user_list
           |
           |__*user
               |
               |__mode
```

# httpServer



1. 数据库创建

```sql
create table login (
	id int primary key auto_increment,
    username varchar(64) not null,
    password varchar(128) not null
);
```

- [x] BASIC FUNCTION COMPLETED
- [ ] README COMPLETED
- [ ] CODE COMMENTS COMPLETED
- [ ] PERFORMANCE TESTING COMPLETED

<br/>
<br/>
<br/>
本项目以学习目的编写，大量参考开源代码<strong><a href="https://github.com/markparticle/WebServer">WebServer</a></strong>、<strong><a href="https://github.com/chenshuo/muduo">muduo</a></strong>以及《Linux高性能服务器编程 - 游双著》，对部分模块进行了调整，并附上了相关注释。


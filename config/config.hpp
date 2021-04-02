#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "toml11/toml.hpp"

#include <string>
#include <iostream>

typedef struct Config{
	std::string		ip;					//服务运行ip
	int				port;				//服务运行端口

	int				trig_mode;			//epoll事件触发方式
	int				timeout_ms;			//timeout延时，ms
	bool			exit_opt;			//退出选项：优雅退出 or 直接关闭

	int 			mysql_port;			/* Mysql连接参数  */
	std::string		mysql_user;
	std::string		mysql_pass;
	std::string		mysql_database_name;

	int				connpool_num;		//http连接池数量
	int				threadpool_num;		//线程池数量
	bool			log_opt;			//日志开关
	int				log_level;			//日志等级
	int				log_queue_size;		//日志异步队列大小

	std::string		resources_src;		//受访问资源路径

	void init_config(){
		//读取配置文件，配置文件的相对路径是相对于可执行文件而言的
		const auto data = toml::parse("config.toml");
		//按表解析，提取配置信息
		const auto& server = toml::find(data, "server");
		ip = toml::find<std::string>(server, "ip");
		port = toml::find<int>(server, "port");
		trig_mode = toml::find<int>(server, "trig_mode");
		timeout_ms = toml::find<int>(server, "timeout_ms");
		exit_opt = toml::find<int>(server, "exit_opt");
		
		const auto& mysql = toml::find(data, "mysql");
		mysql_port = toml::find<int>(mysql, "port");
		mysql_user = toml::find<std::string>(mysql, "user");
		mysql_pass = toml::find<std::string>(mysql, "password");
		mysql_database_name = toml::find<std::string>(mysql, "database");

		const auto& option = toml::find(data, "option");
		connpool_num = toml::find<int>(option, "connpool_num");
		threadpool_num = toml::find<int>(option, "threadpool_num");
		log_opt = toml::find<int>(option, "log_opt");
		log_level = toml::find<int>(option, "log_level");
		log_queue_size = toml::find<int>(option, "log_queue_size");

		const auto& resources = toml::find(data, "resources");
		resources_src = toml::find<std::string>(resources, "src");
	}

	void print(){
		std::cout << ip << "\t" << port << "\n"
		<< trig_mode << "\t" << timeout_ms << "\t" << exit_opt << "\n"
		<< mysql_port << "\t" << mysql_user << "\t" << mysql_pass << "\t" << mysql_database_name << "\n"
		<< connpool_num << "\t" << threadpool_num << "\n" 
		<< log_opt << "\t" << log_level << "\t" << log_queue_size << "\n";
	}
}Config;

#endif

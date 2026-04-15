//
// Created by zhangweize on 26-4-15.
//
#include <json.hpp>
#include <iostream>
#include <string>

using json = nlohmann::json;

struct Person {
    std::string name;
    int age;
    std::string email;

    // 侵入式宏：列出所有需要转换的成员
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Person, name, age, email)
};

int main() {
    // 假设从某处得到了 JSON 字符串
    std::string json_str = R"(
        {

            "age": 28,
            "email": "alice@example.com"
        }
    )";

    // 解析 JSON
    json j = json::parse(json_str);

    // 一步转换：JSON -> Person
    Person p = j.get<Person>();

    std::cout << p.name << ", " << p.age << ", " << p.email << std::endl;
    return 0;
}
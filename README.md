# X-Trader

这是一个基于CTP API的交易系统框架，支持多种交易策略和市场数据处理功能。系统设计模块化，便于扩展和维护。

交流请加vx: X_Trader_Lab

## 特性

- 支持多种交易策略：包括做市商策略、统计套利策略、订单流策略等
- 支持市场数据订阅和处理
- 提供订单管理和执行功能
- 支持策略回测和实时交易
- 跨平台支持（Windows和Linux）

## 目录结构

- `api/` - CTP API相关头文件和库文件
- `bin/` - 配置文件目录
- `src/` - 源代码目录
  - `framework/` - 核心框架代码
  - `strategy/` - 交易策略实现
  - `trade-core/` - 交易核心模块
  - `utils/` - 工具类代码

## 使用方法

1. 配置CTP API环境
2. 修改配置文件以适应您的交易账户信息
3. 选择或实现所需的交易策略
4. 编译并运行系统

## 策略示例

系统包含多种预实现策略：
- `decline_scalping` - 下降趋势套利策略
- `high_low` - 高低价策略
- `market_correction` - 市场回调策略
- `statistical_arbitrage` - 统计套利策略

## 开发者指南

如需添加新策略，请继承`strategy`基类并实现相应的事件处理方法，如`on_tick()`, `on_bar()`, `on_order()`等。

## 交易大屏 UI（V1）

项目已补充盘中交易员场景的交易大屏 UI 方案（固定大屏、深色高信息密度风格），覆盖以下模块：
- 交易账号管理
- 行情源管理
- 策略进程管理 / 交易任务 / 持仓汇总
- 委托记录 / 成交记录（当日）
- 行情展示（十档盘口 + 逐笔）与下单/撤单面板

### 文档索引
- `doc/ui/trading-desk-v1-design.md`：页面结构、模块规格、字段清单、交互规则、刷新策略
- `doc/ui/trading-desk-v1-flows.md`：关键操作流程（登录、订阅、启停、下单撤单、异常处理）
- `doc/ui/trading-desk-v1-data-contract.md`：UI 字段与 C++ 模型映射、事件模型、错误语义
- `doc/ui/trading-desk-v1-frontend-plan.md`：Vue3 + Element Plus 落地脚手架与分阶段实现计划

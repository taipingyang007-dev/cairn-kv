#include "ExpireDict.h"
#include <functional>

ExpireDict::ExpireDict() : ExpireArr_{} {
  // 构造函数初始化实际上就是初始化ExpireArr_
  // ExpireArr_{} 之后是 全是 nullptr

  // 创建虚拟头结点
  for (auto &ExNode : ExpireArr_) {
    try {
      ExpireDictNode *Node = new ExpireDict::ExpireDictNode;
      ExNode = Node;
    } catch (...) {
      reset(); // 释放已经创建成功的头节点
      throw;   // 把原异常继续交给调用方
    }
  }
}

// 析构函数需要析构所有的桶为 nullptr 并释放资源
ExpireDict::~ExpireDict() { reset(); }

void ExpireDict::reset() {
  for (auto &ExNode : ExpireArr_) {
    // 每一个过期 Node 视作一个链表的头节点，目标是释放所有的
    if (ExNode != nullptr) {
      // 需要释放，while循环逐个释放
      while (ExNode != nullptr) {
        auto *temp = ExNode;
        ExNode = ExNode->next;
        delete temp;
        // 这里离开这个while循环就析构了，所以不置为nullptr了
      }
    }
  }
}

// 返回查询结果，如果存在{true，目标节点的前驱}，不存在{false，最后一个节点}
std::pair<bool, ExpireDict::ExpireDictNode *>
ExpireDict::findPrev(const std::string &key) {
  // 找到当前key所需要存放的桶位置
  std::size_t idx = std::hash<std::string>{}(key) % ExpireArr_.size();
  // 从头节点向后查询，如果没有找到同一个key，就不存在
  auto *dummyNode = ExpireArr_[idx];
  while (dummyNode != nullptr && dummyNode->next != nullptr) {
    if (dummyNode->next->key != key) {
      dummyNode = dummyNode->next;
    } else {
      // 找到对应的key，返回目标节点的前驱
      return {true, dummyNode};
    }
  }

  // 离开循环说明此时的 dummyNode->next == nullptr 指向了末尾，不存在
  return {false, dummyNode};
}

// 添加/更新 ExpireDictNode，添加的方式是{key - times}
void ExpireDict::addExpireDictNode(std::string key, times expire_ms) {
  // 找到目标节点 key 的前驱
  auto prev = findPrev(key);
  if (prev.first) {
    // 存在同一个 key，进行修改
    prev.second->next->expire_at_ms = expire_ms;
  } else {
    // 说明key不存在，进行添加
    ExpireDictNode *Node =
        new ExpireDict::ExpireDictNode({key, expire_ms, nullptr});
    prev.second->next = Node;
  }
}

// 删除
bool ExpireDict::delExpireDictNode(std::string key) {
  // 找到目标节点 key 的前驱
  auto prev = findPrev(key);
  if (prev.first) {
    // 存在同一个 key，进行删除
    auto *tempNode = prev.second->next; // 保存目标节点
    prev.second->next = prev.second->next->next;
    // 删除目标节点
    delete tempNode;
    // 这里离开这个while循环就析构了，所以不置为nullptr了
    return true;
  }

  // 不存在返回false
  return false;
}

// 查询
std::optional<times> ExpireDict::getExpireDictNode(std::string key) {
  auto prev = findPrev(key);
  if (prev.first) {
    // key存在，返回过期时间
    return prev.second->next->expire_at_ms;
  }

  // key 不存在
  return std::nullopt;
}
#include<iostream>
#include<vector>
#include"internal/ThreadCache.h"
struct node {
	int x, y;
	char s;
	std::string t;
};
int main(){
  auto temp=alloca_obj<node>();//申请
  free_obj(temp);
}

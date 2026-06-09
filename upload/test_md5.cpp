// 临时测试：验证 MD5 实现是否正确
// 用法：编译后运行，对比系统 md5sum 输出
// g++ -o test_md5 upload/md5.cpp upload/test_md5.cpp -I. && ./test_md5

#include <stdio.h>
#include <string.h>
#include "md5.h"

int main()
{
    // ---- 测试 1：已知答案的空字符串 ----
    // MD5("") 应该是 d41d8cd98f00b204e9800998ecf8427e
    {
        MD5_CTX ctx;
        MD5_Init(&ctx);
        unsigned char input[] = "";
        MD5_Update(&ctx, input, 0);
        unsigned char digest[16];
        MD5_Final(digest, &ctx);

        printf("MD5(\"\"):               ");
        for (int i = 0; i < 16; i++) printf("%02x", digest[i]);
        printf("\n");
        printf("期待:                    d41d8cd98f00b204e9800998ecf8427e\n\n");
    }

    // ---- 测试 2：你的文本文件 ----
    std::string result = md5_file("/home/wwy/text/text.txt");
    printf("md5_file(text.txt):      %s\n", result.c_str());
    printf("系统 md5sum 输出:         6800898c9fe49dc8426da31d0ceb55fd\n\n");

    // ---- 测试 3：用本地数据模拟文件内容 ----
    {
        MD5_CTX ctx;
        MD5_Init(&ctx);
        const char *data = "abcdefghsjdsakjhhsjksajcksakncasncaskdsadsa";
        MD5_Update(&ctx, (const unsigned char *)data, strlen(data));
        unsigned char digest[16];
        MD5_Final(digest, &ctx);

        printf("MD5(内存中的数据):        ");
        for (int i = 0; i < 16; i++) printf("%02x", digest[i]);
        printf("\n");
        printf("期待:                    6800898c9fe49dc8426da31d0ceb55fd\n");
    }

    return 0;
}

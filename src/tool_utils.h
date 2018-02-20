#ifndef S3TOOLS_TOOL_UTILS_H
#define S3TOOLS_TOOL_UTILS_H

#include <s3tools/cred_manage.h>

s3tools::credential findCredentials(const CredentialCollection& credentials,
                                    const std::string& urlStr);

#endif //S3TOOLS_TOOL_UTILS_H
from api import spark_api as spark
import os

class XunfeiLLM():
    def __init__(self):
        self.text = []
        #self.getText("system","你现在扮演李白，你豪情万丈，狂放不羁；接下来请用李白的口吻和用户对话。")
        #print(self.text)

    def set_prompt(self,prompt_text):
        self.text = []
        self.getText("system",prompt_text)

    def getText(self, role, content):
        jsoncon = {}
        jsoncon["role"] = role
        jsoncon["content"] = content
        self.text.append(jsoncon)
        return self.text

    def getlength(self,text):
        length = 0
        for content in text:
            temp = content["content"]
            leng = len(temp)
            length += leng
        return length

    def checklen(self,text):
        while (self.getlength(text) >1000): #8000): #这里缓存不用过长，可能会增加响应时间
            if text[0]["role"] == "system":
                del text[1]  # 删除最旧的一条，注意索引0如果是系统提示词,不要删除
            else:
                del text[0]

        return text

    def chat(self, question):
        # appid、api_secret、api_key三个服务认证信息请前往开放平台控制台查看（https://console.xfyun.cn/services/bm35）
        appid = os.environ.get('XUNFEI_APPID')     #填写控制台中获取的 APPID 信息
        api_key = os.environ.get('XUNFEI_APIKEY')    #填写控制台中获取的 APIKey 信息
        api_secret = os.environ.get('XUNFEI_APISECRET')   #填写控制台中获取的 APISecret 信息

        if appid==None or api_key==None or api_secret==None:
            print("Please set environment variables: XUNFEI_APPID,XUNFEI_APIKEY,XUNFEI_APISECRET!!!")
            return "请将科大迅飞大模型spark lite的api key, api secret, app id设置到环境变量"

        query = self.checklen(self.getText("user", question))
        #print("query=",query)

        # 用于配置大模型版本，默认“general/generalv2”
        # domain = "general"   # v1.5版本
        # domain = "generalv2"    # v2.0版本
        # domain = "generalv3.5"  # 3.5版本
        domain = "lite"   # Spark Lite 官方文档：v1.1/chat 对应 domain=lite（勿用 general，会 11200）
        # 云端环境的服务地址
        # Spark_url = "ws://spark-api.xf-yun.com/v1.1/chat"  # v1.5环境的地址
        # spark_url = "ws://spark-api.xf-yun.com/v2.1/chat"  # v2.0环境的地址
        # spark_url = "wss://spark-api.xf-yun.com/v3.5/chat"  # v3.5环境的地址
        spark_url = "wss://spark-api.xf-yun.com/v1.1/chat"  # Spark Lite 官方文档地址（须 wss）

        answer = spark.main(appid, api_secret, api_key, spark_url, domain, query)
        if answer is None:
            answer = ""
        answer = str(answer).strip()
        if answer:
            self.getText("assistant", answer)

        if answer.startswith("小白："):
            answer = answer[3:].lstrip()

        return answer

def create_llm(type):
    if type == "xunfei":
        llm_client = XunfeiLLM()
    else:
        print("llm type error!")
        exit(-1)

    return llm_client


if __name__ == '__main__':
    llm_client = create_llm("xunfei")
    #question = input("请输入问题：")
    text_input = "你是谁"
    answer = llm_client.chat(text_input)
    print("answer=",answer)

    
    

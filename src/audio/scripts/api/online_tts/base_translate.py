import httpx
# from retrying import retry

class BaseTranslate(object):
    """翻译的基类"""
    def __init__(self):
        # 创建客户端并设置客户端UA
        self.session = httpx.Client(verify=False)
        self.session.headers['User-Agent'] = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) ' \
                                             'AppleWebKit/537.36 (KHTML, like Gecko) ' \
                                             'Chrome/111.0.0.0 Safari/537.36'
        # 主页
        self.home = None

    #@retry(stop_max_attempt_number=3)
    def _get(self, path='', params=None, headers=None):
        """发送 GET 请求"""
        url = self.home + path
        try:
            response = self.session.get(url, params=params, headers=headers, timeout=5.0)
        except httpx.TimeoutException:
            print("TTS HTTP请求超时 5秒")
            return None
        except httpx.HTTPStatusError as exc:
            print(f"TTS HTTP错误: {exc}")
            return None
        except httpx.RequestError as exc:
            print(f"TTS HTTP请求错误: {exc}")
            return None

        return response
        
    def get_tts(self, text, lan, *args, **kwargs) -> bytes:
        """ 获取发音 """
        pass

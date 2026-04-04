import requests, json
import datetime
import pytz
import os

#from api import tts

def get_current_time():
    tz = pytz.timezone("Asia/Shanghai")
    now = datetime.datetime.now(tz)
    #formatted_time = now.strftime("%Y-%m-%d %H:%M:%S")
    time_str = now.strftime("现在时间%H点%M分")
    print("时间：", time_str)
    return time_str

def get_city_name():
    try:
        # 使用一个免费的IP定位API
        response = requests.get('http://ip-api.com/json/?lang=zh-CN',timeout=(2,2))
        data = response.json()
        #print("IP地址API数据:",data)
        if data['status'] == 'success':
            city = data['city']
            return city
        else:
            print('无法获取城市信息')
            return None
    except Exception as e:
        print(f'获取城市发生错误: {e}')
        return None

def get_weather_info(city):
    if city==None:
        return "地理位置获取错误，请重新查询"

    url = 'http://t.weather.sojson.com/api/weather/city/'

    current_dir = os.path.dirname(os.path.realpath(__file__))
    city_json_file_name = os.path.join(current_dir,"..","cfg","city.json")
    if not os.path.exists(city_json_file_name):
        return "城市代码数据文件不存在"


    f = open(city_json_file_name, 'r')
    cities = json.load(f)
    city_id = cities.get(city)
    #print("城市代码:",city_id)
    if city_id == None:
        return "无法查询"+city+"地区的天气，请更换地区查询"

    # 网络请求，传入请求api+城市代码
    try:
        response = requests.get(url + city_id, timeout=(2,2))
        data = response.json()
        #with open("天气.json","w") as f:
        #    json.dump(data,f)
        #print("天气数据:",data)
        if (data['status'] == 200):

            weather_type = data["data"]["forecast"][0]["type"]
            wendu = data["data"]["wendu"]
            shidu = data["data"]["shidu"]
            aqi = data["data"]["forecast"][0]["aqi"]
            quality = data["data"]["quality"]
            ganmao = data["data"]["ganmao"]

            low = data["data"]["forecast"][0]["low"]
            high = data["data"]["forecast"][0]["high"]
            fx = data["data"]["forecast"][0]["fx"]
            fl = data["data"]["forecast"][0]["fl"]
            notice = data["data"]["forecast"][0]["notice"]

            info_str = f"{city}今天天气{weather_type}，温度{wendu}度，{fx}{fl}，空气质量指数{aqi}，空气质量{quality}，小白提示您{notice}"

            return info_str
        else:
            return "天气查询失败"
    except Exception as e:
        print(f'天气查询发生错误: {e}')
        return "天气查询请求错误"


# 'cityInfo': {'city': '成都市', 'citykey': '101270101', 'parent': '四川', 'updateTime': '17:32'},
# 'data': {'shidu': '77%', 'pm25': 42.0, 'pm10': 53.0, 'quality': '良', 'wendu': '21.3', 'ganmao': '极少数敏感人群应减少户外活动',
#          'forecast': [{'date': '03', 'high': '高温 20℃', 'low': '低温 16℃', 'ymd': '2024-11-03', 'week': '星期日',
#                        'sunrise': '07:20', 'sunset': '18:14', 'aqi': 35, 'fx': '北风', 'fl': '1级', 'type': '阴', 'notice': '不要被阴云遮挡住好心情'}

if __name__ == '__main__':
    city = get_city_name()
    print("city=",city)
    weather_info = get_weather_info(city)
    print("weather_info=",weather_info)
    # tts_client = tts.create_tts("youdao")
    # # kaldi tts不会返回mp3数据
    # mp3_data = tts_client.text_to_speech(weather_info, use_database=False)
    # if mp3_data != None:
    #     with open("tts.mp3", "wb") as file:  # 默认存放在.ros里
    #         file.write(mp3_data)
    #     os.system("play tts.mp3")


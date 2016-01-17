#include <Arduino.h>
#include <HttpClient.h>

class GrowthForecastClient {
	HttpClient& http;
	const char* host;
	const char* user;
	const char* pass;
public:
	GrowthForecastClient(HttpClient& client, const char* h, const char* u, const char* p): http(client) {
		host = h;
		user = u;
		pass = p;
	}

	bool post(const char* path, const int32_t v) const {
		int err;

		unsigned long time = millis();
		Serial.printf("post_to_gf to %s with %d\n", path, v);

		String apiPath = "/api";
		apiPath.concat(path);
		apiPath.concat("?number=");
		apiPath.concat(v);

		http.setHttpResponseTimeout(10 * 1000);
		http.beginRequest();
		err = http.post(host, apiPath.c_str());
		if (err) {
			Serial.println("Failed to post (startRequest failed)");
			return 0;
		}
		Serial.printf("post_to_gf: connected, %dms\n", millis() - time);

		http.sendBasicAuth(user, pass);
		// Serial.printf("post_to_gf: sent Authorization %dms\n", millis() - time);

		// http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
		// Serial.printf("post_to_gf: sent Content-Type %dms\n", millis() - time);

		/* why toooo slow
		http.sendHeader("Content-Length", String(params.length()).c_str());
		Serial.printf("post_to_gf: sent Content-Length %dms\n", millis() - time);
		String params = "number=";

		params += v;
		http.println(params);
		Serial.printf("post_to_gf: sent body %dms\n", millis() - time);
		*/

		http.endRequest();

		Serial.printf("post_to_gf: end request %dms\n", millis() - time);

		int code = http.responseStatusCode();
		if (code != 200) {
			Serial.print("http.responseStatusCode returns: ");
			Serial.println(code);

			char c;
			while (http.connected() || http.available() ) {
				c = http.read();
				Serial.print(c);
			}

			return 0;
		}
		Serial.printf("post_to_gf: response status code %d %dms\n", code, millis() - time);

		http.stop();
		Serial.printf("post_to_gf done %dms\n", millis() - time);
		return 1;
	}
};



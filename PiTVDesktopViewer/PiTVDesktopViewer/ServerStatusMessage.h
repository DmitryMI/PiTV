#include <QJsonDocument>
#include <QJsonObject.h>

class ServerStatusMessage
{
public:
	bool isLoadCpuProcessOk;
	double loadCpuProcess;

	bool isLoadCpuTotalOk;
	double loadCpuTotal;

	bool isTemperatureCpuOk;
	double temperatureCpu;

    template <typename T>
    static bool readValue(const QJsonObject& root, QString name, QVariant& out_result)
    {
        QJsonValue json_value = root.value(name);
        if (!json_value.isNull())
        {
            return false;
        }
        
        out_result = json_value.toVariant();

        return true;
    }

    static bool readValue(const QJsonObject& root, QString name, bool& out_result)
    {
        QJsonValue json_value = root.value(name);
        if (!json_value.isNull())
        {
            return false;
        }

        out_result = json_value.toBool();

        return true;
    }

    static bool readValue(const QJsonObject& root, QString name, double& out_result)
    {
        QJsonValue json_value = root.value(name);
        if (!json_value.isNull())
        {
            return false;
        }

        out_result = json_value.toDouble();

        return true;
    }

	static bool fromJson(QString jsonText, ServerStatusMessage& message)
	{
        QJsonDocument d = QJsonDocument::fromJson(jsonText.toUtf8());
        QJsonObject root = d.object();

        if (!readValue(root, "load_cpu_process_ok", message.isLoadCpuProcessOk))
        {
            return false;
        }

        if (!readValue(root, "load_cpu_process", message.loadCpuProcess))
        {
            return false;
        }

        if (!readValue(root, "load_cpu_total_ok", message.isLoadCpuTotalOk))
        {
            return false;
        }

        if (!readValue(root, "load_cpu_total", message.loadCpuTotal))
        {
            return false;
        }

        if (!readValue(root, "temp_cpu_ok", message.isTemperatureCpuOk))
        {
            return false;
        }

        if (!readValue(root, "temp_cpu", message.temperatureCpu))
        {
            return false;
        }

        return true;
	}
};
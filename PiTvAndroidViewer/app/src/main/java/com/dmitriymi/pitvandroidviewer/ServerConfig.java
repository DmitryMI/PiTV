package com.dmitriymi.pitvandroidviewer;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

import java.io.Serializable;

public class ServerConfig implements Parcelable
{
    String serverUrl;
    String username;
    String password;
    String localUdpEndpoint;
    String tlsCaData;
    String tlsClientPublicKeyData;
    String tlsClientPrivateKeyData;

    public ServerConfig()
    {

    }

    protected ServerConfig(Parcel in) {
        serverUrl = in.readString();
        username = in.readString();
        password = in.readString();
        localUdpEndpoint = in.readString();
        tlsCaData = in.readString();
        tlsClientPublicKeyData = in.readString();
        tlsClientPrivateKeyData = in.readString();
    }

    public static final Creator<ServerConfig> CREATOR = new Creator<ServerConfig>() {
        @Override
        public ServerConfig createFromParcel(Parcel in) {
            return new ServerConfig(in);
        }

        @Override
        public ServerConfig[] newArray(int size) {
            return new ServerConfig[size];
        }
    };

    public String getServerUrl() {
        return serverUrl;
    }

    public void setServerUrl(String serverUrl) {
        this.serverUrl = serverUrl;
    }

    public String getUsername() {
        return username;
    }

    public void setUsername(String username) {
        this.username = username;
    }

    public String getPassword() {
        return password;
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public String getLocalUdpEndpoint() {
        return localUdpEndpoint;
    }

    public void setLocalUdpEndpoint(String localUdpEndpoint) {
        this.localUdpEndpoint = localUdpEndpoint;
    }

    public String getTlsCaData() {
        return tlsCaData;
    }

    public void setTlsCaData(String tlsCaData) {
        this.tlsCaData = tlsCaData;
    }

    public String getTlsClientPublicKeyData() {
        return tlsClientPublicKeyData;
    }

    public void setTlsClientPublicKeyData(String tlsClientPublicKeyData) {
        this.tlsClientPublicKeyData = tlsClientPublicKeyData;
    }

    public String getTlsClientPrivateKeyData() {
        return tlsClientPrivateKeyData;
    }

    public void setTlsClientPrivateKeyData(String tlsClientPrivateKeyData) {
        this.tlsClientPrivateKeyData = tlsClientPrivateKeyData;
    }

    public int getLocalUdpPort()
    {
        if(localUdpEndpoint.isEmpty())
        {
            return -1;
        }

        String[] ipParts = localUdpEndpoint.split(":");
        if(ipParts.length == 1)
        {
            return 80;
        }
        return Integer.parseInt(ipParts[1]);
    }

    public String getLocalUdpAddress()
    {
        if(localUdpEndpoint.isEmpty())
        {
            return "";
        }

        String[] ipParts = localUdpEndpoint.split(":");
        return ipParts[0];
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
        dest.writeString(serverUrl);
        dest.writeString(username);
        dest.writeString(password);
        dest.writeString(localUdpEndpoint);
        dest.writeString(tlsCaData);
        dest.writeString(tlsClientPublicKeyData);
        dest.writeString(tlsClientPrivateKeyData);
    }
}

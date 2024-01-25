//
//  ContentView.swift
//  BabyTimer Watch App
//
//  Created by Anatolii Kasianov on 18.11.2023.
//

import SwiftUI

struct ContentView: View {
    
    @State private var timeFromServer = ""
    
    var body: some View {
        
        let timer = Timer.publish(every: 2, on: .current, in: .common).autoconnect()
        
        VStack {
            Text(randomEmoji())
                .font(.title)
            Text("\(timeFromServer)")
                    .onReceive(timer) { input in
                        executeGetElapsedTimeRequest()
                    }
                    .font(.title)
            Button("Reset", action: executeResetRequest)
                .font(.title)
        }
        .padding()
    }
    
    func executeGetElapsedTimeRequest() {
        print("executeGetElapsedTimeRequest")
        let url = URL(string: "http://babytimer.local:1880/elapsed-time")!
        let request = URLRequest(url: url, timeoutInterval: 15)
        let task = URLSession.shared.dataTask(with: request) {(data, response, error) in
            guard let data = data else { return }
            print(String(data: data, encoding: .utf8)!)
            self.timeFromServer = String(data: data, encoding: .utf8)!
            print(self.timeFromServer)
        }
        task.resume()
    }
    
    func executeResetRequest() {
        print("executeResetRequest")
        self.timeFromServer = "00:00:00"
        let url = URL(string: "http://babytimer.local:1880/reset-time")!
        let request = URLRequest(url: url, timeoutInterval: 15)
        let task = URLSession.shared.dataTask(with: request) {(data, response, error) in
            guard let data = data else { return }
            print(String(data: data, encoding: .utf8)!)
        }
        task.resume()
    }
    
    func randomEmoji() -> String {
        let letters = Array("🍔🌭🌮🌯🥪🍕🍟🍖🍜🍩🍪🥟🍦🧆🥓🍗🍝🍣🍱🥘🍳🥞🧇🍧🍮🥧🥐")
        var s = ""
        for _ in 0 ..< 1 {
            s.append(letters.randomElement()!)
        }
        return s
    }
}

#Preview {
    ContentView()
}

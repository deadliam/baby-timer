//
//  ContentView.swift
//  BabyTimer
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
        let url = URL(string: "http://babytimer.local:1880/elapsed-time")!
        let task = URLSession.shared.dataTask(with: url) {(data, response, error) in
            guard let data = data else { return }
//            print(String(data: data, encoding: .utf8)!)
            self.timeFromServer = String(data: data, encoding: .utf8)!
        }
        task.resume()
    }
    
    func executeResetRequest() {
        print("executeResetRequest")
        self.timeFromServer = "00:00:00"
        let url = URL(string: "http://babytimer.local:1880/reset-time")!
        let task = URLSession.shared.dataTask(with: url) {(data, response, error) in
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

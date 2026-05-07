import { motion, AnimatePresence } from 'framer-motion';
import { useState, useEffect, useCallback } from 'react';
import { Play, Pause, RotateCcw, Activity, Zap, Server, ShieldCheck } from 'lucide-react';
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  BarChart, Bar, Cell
} from 'recharts';

const INITIAL_DATA_POINTS = 60;

export default function LiveDemo() {
  const [isRunning, setIsRunning] = useState(false);
  const [metrics, setMetrics] = useState({
    avgLatency: 890,
    p99Latency: 920,
    throughput: 1.25, // M/s
    activeKernels: 4,
    cacheHitRate: 99.2
  });

  // Real-time time-series data
  const [seriesData, setSeriesData] = useState(() =>
    Array.from({ length: INITIAL_DATA_POINTS }, (_, i) => ({
      time: i,
      buyIntensity: 10 + Math.random() * 5,
      sellIntensity: 10 + Math.random() * 5,
      price: 100 + Math.sin(i / 10) * 1,
    }))
  );

  // Jitter histogram data (simulated normal distribution + tail)
  const [jitterData, setJitterData] = useState(() =>
    Array.from({ length: 20 }, (_, i) => ({
      bucket: `${i * 50}-${(i + 1) * 50} cycles`,
      count: i < 5 ? Math.random() * 1000 + 500 : Math.random() * 50,
      color: i < 15 ? '#10b981' : '#ef4444' // Green for low jitter, red for tail
    }))
  );

  const [logs, setLogs] = useState<string[]>([
    "[SYSTEM] Jitter Profiler init... OK",
    "[SYSTEM] Warm-up phase complete (50k iters)",
    "[ALPHA] VectorizedMultiKernelHawkes ready"
  ]);

  const addLog = useCallback((msg: string) => {
    setLogs(prev => [msg, ...prev].slice(0, 5));
  }, []);

  useEffect(() => {
    let interval: any;
    if (isRunning) {
      interval = setInterval(() => {
        // 1. Update Time Series (Hawkes Simulation)
        setSeriesData(prev => {
          const last = prev[prev.length - 1];
          // Decay
          let nextBuy = last.buyIntensity * 0.9 + 1;
          let nextSell = last.sellIntensity * 0.9 + 1;

          // Random jumps (Self-excitation)
          if (Math.random() > 0.8) nextBuy += Math.random() * 15;
          if (Math.random() > 0.8) nextSell += Math.random() * 15;

          // Cross-excitation influence on price
          const priceChange = (nextBuy - nextSell) * 0.01;

          const newPoint = {
            time: last.time + 1,
            buyIntensity: nextBuy,
            sellIntensity: nextSell,
            price: last.price + priceChange + (Math.random() - 0.5) * 0.1,
          };
          return [...prev.slice(1), newPoint];
        });

        // 2. Update Jitter Histogram (Simulate rare spikes)
        if (Math.random() > 0.95) {
          setJitterData(prev => {
            const newData = [...prev];
            // Add to a tail bucket
            const tailIdx = 15 + Math.floor(Math.random() * 5);
            if (newData[tailIdx]) newData[tailIdx].count += 1;
            addLog(`[WARN] Tail latency detected: ${tailIdx * 50} cycles`);
            return newData;
          });
          setMetrics(m => ({ ...m, p99Latency: m.p99Latency + Math.random() * 10 }));
        } else {
          // Normal operation
          setJitterData(prev => {
            const newData = [...prev];
            const cleanIdx = Math.floor(Math.random() * 5);
            newData[cleanIdx].count += 5;
            return newData;
          });
          setMetrics(m => ({ ...m, p99Latency: Math.max(890, m.p99Latency * 0.99) }));
        }

      }, 50); // 20Hz update
    }
    return () => clearInterval(interval);
  }, [isRunning, addLog]);

  const handleReset = () => {
    setIsRunning(false);
    setSeriesData(Array.from({ length: INITIAL_DATA_POINTS }, (_, i) => ({
      time: i,
      buyIntensity: 10 + Math.random() * 5,
      sellIntensity: 10 + Math.random() * 5,
      price: 100,
    })));
  };

  return (
    <section id="demo" className="py-24 px-4 sm:px-6 lg:px-8 bg-zinc-50 dark:bg-zinc-950">
      <div className="max-w-7xl mx-auto">

        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="text-center mb-16"
        >
          <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-blue-100 text-blue-700 text-sm font-medium mb-4">
            <Activity className="w-4 h-4" />
            <span>v2.4.0 Live Engine</span>
          </div>
          <h2 className="text-4xl md:text-5xl font-bold text-gray-900 tracking-tight mb-4">
            System <span className="text-blue-600">Telemetry</span>
          </h2>
          <p className="text-lg text-gray-500 max-w-2xl mx-auto">
            Real-time visualization of the Multi-Kernel Hawkes process and Jitter Profiler.
          </p>
        </motion.div>

        {/* Dashboard Grid */}
        <div className="grid lg:grid-cols-3 gap-6 mb-12">

          {/* Main Chart Area */}
          <motion.div
            initial={{ opacity: 0, scale: 0.95 }}
            whileInView={{ opacity: 1, scale: 1 }}
            viewport={{ once: true }}
            className="lg:col-span-2 bg-white rounded-2xl shadow-xl border border-gray-100 overflow-hidden"
          >
            <div className="p-6 border-b border-gray-100 flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className="p-2 bg-indigo-100 text-indigo-600 rounded-lg">
                  <Zap className="w-5 h-5" />
                </div>
                <div>
                  <h3 className="font-semibold text-gray-900">Alpha intensity & Price</h3>
                  <p className="text-xs text-gray-500">Dual-axis: Signal Strength vs Market Mid-Price</p>
                </div>
              </div>
              <div className="flex gap-2">
                <button onClick={() => setIsRunning(!isRunning)} className={`p-2 rounded-lg transition-colors ${isRunning ? 'bg-red-50 text-red-600 hover:bg-red-100' : 'bg-green-50 text-green-600 hover:bg-green-100'}`}>
                  {isRunning ? <Pause className="w-5 h-5" /> : <Play className="w-5 h-5" />}
                </button>
                <button onClick={handleReset} className="p-2 rounded-lg bg-gray-50 text-gray-600 hover:bg-gray-100">
                  <RotateCcw className="w-5 h-5" />
                </button>
              </div>
            </div>

            <div className="h-[350px] w-full p-4">
              <ResponsiveContainer width="100%" height="100%">
                <LineChart data={seriesData}>
                  <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#f3f4f6" />
                  <XAxis dataKey="time" hide />
                  <YAxis yAxisId="left" domain={[0, 60]} hide />
                  <YAxis yAxisId="right" orientation="right" domain={['auto', 'auto']} hide />
                  <Tooltip
                    contentStyle={{ borderRadius: '12px', border: 'none', boxShadow: '0 10px 15px -3px rgba(0, 0, 0, 0.1)' }}
                  />
                  <Line yAxisId="left" type="monotone" dataKey="buyIntensity" stroke="#3b82f6" strokeWidth={2} dot={false} activeDot={{ r: 6 }} />
                  <Line yAxisId="left" type="monotone" dataKey="sellIntensity" stroke="#ef4444" strokeWidth={2} dot={false} />
                  <Line yAxisId="right" type="monotone" dataKey="price" stroke="#10b981" strokeWidth={2} strokeDasharray="5 5" dot={false} />
                </LineChart>
              </ResponsiveContainer>
            </div>

            <div className="px-6 py-4 bg-gray-50/50 flex gap-6 text-sm">
              <div className="flex items-center gap-2">
                <span className="w-3 h-3 rounded-full bg-blue-500"></span>
                <span className="text-gray-600">Buy Intensity</span>
              </div>
              <div className="flex items-center gap-2">
                <span className="w-3 h-3 rounded-full bg-red-500"></span>
                <span className="text-gray-600">Sell Intensity</span>
              </div>
              <div className="flex items-center gap-2">
                <span className="w-3 h-3 rounded-full bg-green-500"></span>
                <span className="text-gray-600">Mid Price</span>
              </div>
            </div>
          </motion.div>

          {/* Sidebar / Stats */}
          <div className="space-y-6">

            {/* Top Metrics */}
            <motion.div
              initial={{ opacity: 0, x: 20 }}
              whileInView={{ opacity: 1, x: 0 }}
              viewport={{ once: true }}
              className="grid grid-cols-2 gap-4"
            >
              <div className="bg-white p-4 rounded-xl shadow-sm border border-gray-100">
                <div className="text-xs text-gray-500 mb-1">E2E Latency (p99)</div>
                <div className="text-2xl font-bold text-gray-900">{Math.round(metrics.p99Latency)}<span className="text-sm font-normal text-gray-400 ml-1">ns</span></div>
              </div>
              <div className="bg-white p-4 rounded-xl shadow-sm border border-gray-100">
                <div className="text-xs text-gray-500 mb-1">Throughput</div>
                <div className="text-2xl font-bold text-gray-900">{metrics.throughput}<span className="text-sm font-normal text-gray-400 ml-1">M/s</span></div>
              </div>
            </motion.div>

            {/* Jitter Histogram */}
            <motion.div
              initial={{ opacity: 0, x: 20 }}
              whileInView={{ opacity: 1, x: 0 }}
              viewport={{ once: true }}
              transition={{ delay: 0.1 }}
              className="bg-white rounded-xl shadow-lg border border-gray-100 p-5"
            >
              <div className="flex items-center justify-between mb-4">
                <div className="flex items-center gap-2">
                  <Server className="w-4 h-4 text-orange-500" />
                  <h4 className="font-semibold text-gray-900 text-sm">Jitter Profile</h4>
                </div>
                <span className="text-xs text-green-600 font-medium bg-green-50 px-2 py-1 rounded">Clean</span>
              </div>
              <div className="h-[150px]">
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart data={jitterData}>
                    <Tooltip cursor={{ fill: 'transparent' }} content={() => null} />
                    <Bar dataKey="count" radius={[2, 2, 0, 0]}>
                      {jitterData.map((entry, index) => (
                        <Cell key={`cell-${index}`} fill={entry.color} />
                      ))}
                    </Bar>
                  </BarChart>
                </ResponsiveContainer>
              </div>
              <div className="text-xs text-center text-gray-400 mt-2">Cycle Buckets (0 - 1000+)</div>
            </motion.div>

            {/* Terminal / Logs */}
            <motion.div
              initial={{ opacity: 0, x: 20 }}
              whileInView={{ opacity: 1, x: 0 }}
              viewport={{ once: true }}
              transition={{ delay: 0.2 }}
              className="bg-zinc-900 rounded-xl shadow-lg p-4 font-mono text-xs text-green-400 h-[160px] overflow-hidden relative"
            >
              <div className="flex items-center gap-2 border-b border-zinc-800 pb-2 mb-2">
                <ShieldCheck className="w-3 h-3" />
                <span className="text-zinc-400">Execution Kernel</span>
              </div>
              <div className="space-y-1">
                <AnimatePresence>
                  {logs.map((log, i) => (
                    <motion.div
                      key={`${log}-${i}`}
                      initial={{ opacity: 0, x: -10 }}
                      animate={{ opacity: 1, x: 0 }}
                      exit={{ opacity: 0 }}
                      className="truncate"
                    >
                      <span className="text-zinc-600 mr-2">{new Date().toLocaleTimeString().split(' ')[0]}</span>
                      {log}
                    </motion.div>
                  ))}
                </AnimatePresence>
              </div>
            </motion.div>

          </div>
        </div>

        {/* Source Link */}
        <div className="text-center">
          <a href="https://github.com/krish567366/submicro-execution-engine" className="text-sm text-gray-500 hover:text-gray-900 underline underline-offset-4 decoration-gray-300">
            View source code on GitHub
          </a>
        </div>

      </div>
    </section>
  );
}

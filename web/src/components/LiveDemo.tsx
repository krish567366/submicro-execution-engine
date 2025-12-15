import { motion } from 'framer-motion';
import { useState } from 'react';
import { Play, Pause, RotateCcw } from 'lucide-react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';

export default function LiveDemo() {
  const [isRunning, setIsRunning] = useState(false);
  const [data, setData] = useState(() => 
    Array.from({ length: 50 }, (_, i) => ({
      time: i,
      latency: Math.random() * 100 + 850,
      price: 100 + Math.sin(i / 10) * 2 + Math.random() * 0.5,
    }))
  );

  const handleStart = () => {
    setIsRunning(!isRunning);
    if (!isRunning) {
      const interval = setInterval(() => {
        setData(prev => {
          const newPoint = {
            time: prev[prev.length - 1].time + 1,
            latency: Math.random() * 100 + 850,
            price: prev[prev.length - 1].price + (Math.random() - 0.5) * 0.5,
          };
          return [...prev.slice(1), newPoint];
        });
      }, 100);
      return () => clearInterval(interval);
    }
  };

  return (
    <section id="demo" className="relative py-20 px-4 sm:px-6 lg:px-8 bg-secondary">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="text-center mb-16"
        >
          <h2 className="text-4xl md:text-5xl font-light text-foreground mb-4">
            Live <span className="font-semibold">Simulation</span>
          </h2>
          <p className="text-lg text-muted max-w-3xl mx-auto leading-relaxed">
            Real-time visualization of our execution engine in action.
          </p>
        </motion.div>

        {/* Demo Container */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="p-8 rounded-lg bg-white border border-border"
        >
          {/* Controls */}
          <div className="flex items-center justify-between mb-8">
            <h3 className="text-2xl font-medium text-foreground">Execution Monitor</h3>
            <div className="flex items-center gap-3">
              <button
                onClick={handleStart}
                className={`px-6 py-3 rounded-lg font-medium flex items-center gap-2 transition-all duration-300 ${
                  isRunning
                    ? 'bg-red-500 hover:bg-red-600 text-white'
                    : 'bg-secondary border-2 border-border text-foreground hover:border-foreground/50'
                }`}
              >
                {isRunning ? (
                  <>
                    <Pause className="w-4 h-4" />
                    Stop
                  </>
                ) : (
                  <>
                    <Play className="w-4 h-4" />
                    Start
                  </>
                )}
              </button>
              <button
                onClick={() => setData(Array.from({ length: 50 }, (_, i) => ({
                  time: i,
                  latency: Math.random() * 100 + 850,
                  price: 100 + Math.sin(i / 10) * 2 + Math.random() * 0.5,
                })))}
                className="px-4 py-3 rounded-lg border border-border hover:border-foreground/20 transition-colors"
              >
                <RotateCcw className="w-4 h-4 text-foreground" />
              </button>
            </div>
          </div>

          {/* Charts */}
          <div className="grid lg:grid-cols-2 gap-6">
            {/* Latency Chart */}
            <div className="p-6 rounded-lg bg-secondary border border-border">
              <h4 className="text-lg font-medium text-foreground mb-4">Decision Latency (ns)</h4>
              <ResponsiveContainer width="100%" height={250}>
                <LineChart data={data}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
                  <XAxis dataKey="time" stroke="#6b7280" hide />
                  <YAxis stroke="#6b7280" domain={[800, 1000]} style={{ fontSize: '12px' }} />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: '#ffffff',
                      border: '1px solid #e5e7eb',
                      borderRadius: '8px',
                      boxShadow: '0 4px 6px -1px rgba(0, 0, 0, 0.1)',
                    }}
                  />
                  <Line type="monotone" dataKey="latency" stroke="#3b82f6" strokeWidth={2} dot={false} />
                </LineChart>
              </ResponsiveContainer>
            </div>

            {/* Price Chart */}
            <div className="p-6 rounded-lg bg-secondary border border-border">
              <h4 className="text-lg font-medium text-foreground mb-4">Mid Price ($)</h4>
              <ResponsiveContainer width="100%" height={250}>
                <LineChart data={data}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
                  <XAxis dataKey="time" stroke="#6b7280" hide />
                  <YAxis stroke="#6b7280" domain={[98, 102]} style={{ fontSize: '12px' }} />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: '#ffffff',
                      border: '1px solid #e5e7eb',
                      borderRadius: '8px',
                      boxShadow: '0 4px 6px -1px rgba(0, 0, 0, 0.1)',
                    }}
                  />
                  <Line type="monotone" dataKey="price" stroke="#10b981" strokeWidth={2} dot={false} />
                </LineChart>
              </ResponsiveContainer>
            </div>
          </div>

          {/* Stats */}
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mt-6">
            {[
              { label: 'Avg Latency', value: '892 ns', color: '#3b82f6' },
              { label: 'Throughput', value: '1.2M/s', color: '#10b981' },
              { label: 'Position', value: '+150', color: '#8b5cf6' },
              { label: 'PnL', value: '+$2,450', color: '#f59e0b' },
            ].map((stat, idx) => (
              <div key={idx} className="p-4 rounded-lg bg-secondary border border-border text-center">
                <div className="text-xs text-muted mb-1 font-medium">{stat.label}</div>
                <div className="text-xl font-semibold" style={{ color: stat.color }}>{stat.value}</div>
              </div>
            ))}
          </div>
        </motion.div>

        {/* Download CTA */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="mt-16 text-center"
        >
          <div className="inline-block p-8 rounded-lg bg-white border border-border">
            <h3 className="text-2xl font-medium text-foreground mb-4">Ready to Get Started?</h3>
            <p className="text-muted mb-6 max-w-2xl">
              Download the source code and start building your own ultra-low-latency trading system.
            </p>
            <div className="flex flex-col sm:flex-row gap-4 justify-center">
              <a
                href="https://github.com/krish567366/submicro-execution-engine"
                target="_blank"
                rel="noopener noreferrer"
                className="px-8 py-4 bg-secondary border-2 border-border rounded-lg text-foreground font-medium hover:border-foreground/50 transition-all duration-300"
              >
                View on GitHub
              </a>
              <a
                href="#architecture"
                className="px-8 py-4 border border-border rounded-lg text-foreground font-medium hover:border-foreground/20 transition-all duration-300"
              >
                Learn More
              </a>
            </div>
          </div>
        </motion.div>
      </div>
    </section>
  );
}

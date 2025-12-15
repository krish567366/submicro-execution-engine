import { motion } from 'framer-motion';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Cell } from 'recharts';

const latencyData = [
  { name: 'Market Data', median: 87, p99: 124, color: '#facc15' },
  { name: 'Signal Extract', median: 40, p99: 48, color: '#22c55e' },
  { name: 'Hawkes Update', median: 150, p99: 189, color: '#3b82f6' },
  { name: 'E2E Decision', median: 890, p99: 921, color: '#a855f7' },
  { name: 'Serialization', median: 34, p99: 41, color: '#ec4899' },
];

const comparisonData = [
  { system: 'Traditional HFT', latency: 50000, color: '#ef4444' },
  { system: 'Optimized C++', latency: 5000, color: '#f97316' },
  { system: 'Our System', latency: 890, color: '#22c55e' },
];

export default function Performance() {
  return (
    <section className="relative py-20 px-4 sm:px-6 lg:px-8 bg-white">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="text-center mb-16"
        >
          <h2 className="text-4xl md:text-5xl font-light text-foreground mb-4">
            Nanosecond-Level <span className="font-semibold">Precision</span>
          </h2>
          <p className="text-lg text-muted max-w-3xl mx-auto leading-relaxed">
            Measured with TSC-level accuracy. Every operation optimized for minimal latency.
          </p>
        </motion.div>

        {/* Main Performance Chart */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          transition={{ delay: 0.2 }}
          className="mb-12"
        >
          <div className="p-8 rounded-lg bg-white border border-border">
            <h3 className="text-2xl font-medium text-foreground mb-6">Component Latency Breakdown</h3>
            <ResponsiveContainer width="100%" height={400}>
              <BarChart data={latencyData}>
                <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
                <XAxis dataKey="name" stroke="#6b7280" style={{ fontSize: '14px' }} />
                <YAxis stroke="#6b7280" label={{ value: 'Latency (ns)', angle: -90, position: 'insideLeft', fill: '#6b7280' }} style={{ fontSize: '14px' }} />
                <Tooltip
                  contentStyle={{
                    backgroundColor: '#ffffff',
                    border: '1px solid #e5e7eb',
                    borderRadius: '8px',
                    boxShadow: '0 4px 6px -1px rgba(0, 0, 0, 0.1)',
                  }}
                  labelStyle={{ color: '#0a0a0a', fontWeight: 500 }}
                />
                <Bar dataKey="median" name="Median" radius={[8, 8, 0, 0]}>
                  {latencyData.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={entry.color} />
                  ))}
                </Bar>
                <Bar dataKey="p99" name="p99" radius={[8, 8, 0, 0]} opacity={0.5}>
                  {latencyData.map((entry, index) => (
                    <Cell key={`cell-p99-${index}`} fill={entry.color} />
                  ))}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          </div>
        </motion.div>

        {/* Comparison Section */}
        <div className="grid md:grid-cols-2 gap-8 mb-12">
          {/* Comparison Chart */}
          <motion.div
            initial={{ opacity: 0, x: -20 }}
            whileInView={{ opacity: 1, x: 0 }}
            viewport={{ once: true }}
            className="p-8 rounded-lg bg-white border border-border"
          >
            <h3 className="text-2xl font-medium text-foreground mb-6">Industry Comparison</h3>
            <ResponsiveContainer width="100%" height={300}>
              <BarChart data={comparisonData} layout="vertical">
                <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
                <XAxis type="number" stroke="#6b7280" label={{ value: 'Latency (ns)', position: 'insideBottom', fill: '#6b7280' }} style={{ fontSize: '14px' }} />
                <YAxis dataKey="system" type="category" stroke="#6b7280" width={120} style={{ fontSize: '14px' }} />
                <Tooltip
                  contentStyle={{
                    backgroundColor: '#ffffff',
                    border: '1px solid #e5e7eb',
                    borderRadius: '8px',
                    boxShadow: '0 4px 6px -1px rgba(0, 0, 0, 0.1)',
                  }}
                />
                <Bar dataKey="latency" radius={[0, 8, 8, 0]}>
                  {comparisonData.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={entry.color} />
                  ))}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          </motion.div>

          {/* Stats Cards */}
          <motion.div
            initial={{ opacity: 0, x: 20 }}
            whileInView={{ opacity: 1, x: 0 }}
            viewport={{ once: true }}
            className="space-y-4"
          >
            {[
              { label: 'Median E2E Latency', value: '890 ns', subtext: '±5ns TSC jitter', color: '#3b82f6' },
              { label: 'p99 Latency', value: '921 ns', subtext: 'Consistent performance', color: '#8b5cf6' },
              { label: 'p99.9 Latency', value: '1,047 ns', subtext: 'No tail latency spikes', color: '#10b981' },
            ].map((stat, index) => (
              <div key={index} className="p-6 rounded-lg bg-white border border-border hover:border-foreground/20 transition-all duration-300">
                <div className="flex items-center justify-between mb-2">
                  <span className="text-muted text-sm font-medium">{stat.label}</span>
                  <div className="w-2 h-2 rounded-full" style={{ backgroundColor: stat.color }}></div>
                </div>
                <div className="text-3xl font-semibold mb-1" style={{ color: stat.color }}>
                  {stat.value}
                </div>
                <div className="text-xs text-muted">{stat.subtext}</div>
              </div>
            ))}
          </motion.div>
        </div>

        {/* Measurement Info */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="p-6 rounded-lg bg-secondary border border-border"
        >
          <div className="flex items-start gap-4">
            <div className="p-3 rounded-lg bg-white border border-border">
              <svg className="w-6 h-6 text-foreground" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" />
              </svg>
            </div>
            <div>
              <h4 className="text-lg font-semibold text-foreground mb-2">Measurement Methodology</h4>
              <p className="text-muted text-sm leading-relaxed">
                All measurements taken on Intel Xeon Platinum 8280 @ 2.7GHz, isolated core, RT kernel. 
                Precision: ±5ns (TSC jitter), ±17ns (PTP offset). Bare metal, C-states OFF, Turbo Boost OFF.
              </p>
            </div>
          </div>
        </motion.div>
      </div>
    </section>
  );
}

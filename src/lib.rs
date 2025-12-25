// Rust FFI Interface for C++ HFT System
// Provides zero-cost abstractions and memory safety guarantees
// while maintaining sub-microsecond performance

use std::sync::atomic::{AtomicU64, AtomicBool, Ordering};
use std::time::{Duration, Instant};

// ============================================================================
// FFI-compatible types (matching C++ structs)
// ============================================================================

#[repr(C, align(64))]
pub struct MarketTick {
    pub timestamp_ns: i64,
    pub bid_price: f64,
    pub ask_price: f64,
    pub mid_price: f64,
    pub bid_size: u64,
    pub ask_size: u64,
    pub trade_volume: u64,
    pub trade_side: u8,  // 0 = BUY, 1 = SELL
    pub asset_id: u32,
    pub depth_levels: u8,
    _padding: [u8; 7],
    pub bid_prices: [f64; 10],
    pub ask_prices: [f64; 10],
    pub bid_sizes: [u64; 10],
    pub ask_sizes: [u64; 10],
}

#[repr(C)]
pub struct Order {
    pub order_id: u64,
    pub asset_id: u32,
    pub side: u8,
    pub price: f64,
    pub quantity: u64,
    pub submit_time_ns: i64,
    pub venue_id: u8,
    pub is_active: bool,
    _padding: [u8; 6],
}

// ============================================================================
// Lock-Free SPSC Queue (Rust implementation)
// Provides memory safety + zero-cost abstraction
// ============================================================================

pub struct LockFreeSPSC<T, const CAPACITY: usize> {
    buffer: Box<[T; CAPACITY]>,
    head: AtomicU64,
    tail: AtomicU64,
}

impl<T: Default + Copy, const CAPACITY: usize> LockFreeSPSC<T, CAPACITY> {
    pub fn new() -> Self {
        assert!(CAPACITY.is_power_of_two(), "Capacity must be power of 2");
        
        // Use MaybeUninit for uninitialized array
        let buffer = unsafe {
            let mut array: [T; CAPACITY] = std::mem::MaybeUninit::uninit().assume_init();
            for item in &mut array {
                *item = T::default();
            }
            Box::new(array)
        };
        
        Self {
            buffer,
            head: AtomicU64::new(0),
            tail: AtomicU64::new(0),
        }
    }
    
    /// Producer: Push item (returns false if full)
    #[inline(always)]
    pub fn push(&self, item: T) -> bool {
        let current_tail = self.tail.load(Ordering::Relaxed);
        let next_tail = current_tail.wrapping_add(1);
        
        // Check if full
        if next_tail == self.head.load(Ordering::Acquire) {
            return false;
        }
        
        // Write data
        let idx = (current_tail as usize) & (CAPACITY - 1);
        unsafe {
            let ptr = self.buffer.as_ptr() as *mut T;
            ptr.add(idx).write(item);
        }
        
        // Publish
        self.tail.store(next_tail, Ordering::Release);
        true
    }
    
    /// Consumer: Pop item (returns None if empty)
    #[inline(always)]
    pub fn pop(&self) -> Option<T> {
        let current_head = self.head.load(Ordering::Relaxed);
        
        // Check if empty
        if current_head == self.tail.load(Ordering::Acquire) {
            return None;
        }
        
        // Read data
        let idx = (current_head as usize) & (CAPACITY - 1);
        let item = unsafe {
            let ptr = self.buffer.as_ptr();
            ptr.add(idx).read()
        };
        
        // Advance head
        self.head.store(current_head.wrapping_add(1), Ordering::Release);
        Some(item)
    }
    
    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.head.load(Ordering::Acquire) == self.tail.load(Ordering::Acquire)
    }
    
    #[inline(always)]
    pub fn size(&self) -> usize {
        let h = self.head.load(Ordering::Acquire);
        let t = self.tail.load(Ordering::Acquire);
        ((t.wrapping_sub(h)) as usize) & (CAPACITY - 1)
    }
}

unsafe impl<T: Send, const CAPACITY: usize> Send for LockFreeSPSC<T, CAPACITY> {}
unsafe impl<T: Send, const CAPACITY: usize> Sync for LockFreeSPSC<T, CAPACITY> {}

// ============================================================================
// High-Resolution Timer (Rust-side)
// ============================================================================

pub struct HiResTimer {
    start: Instant,
}

impl HiResTimer {
    #[inline(always)]
    pub fn new() -> Self {
        Self {
            start: Instant::now(),
        }
    }
    
    #[inline(always)]
    pub fn elapsed_ns(&self) -> u64 {
        self.start.elapsed().as_nanos() as u64
    }
    
    #[inline(always)]
    pub fn now_ns() -> i64 {
        // Use TSC (Time Stamp Counter) for lowest latency on x86
        #[cfg(target_arch = "x86_64")]
        unsafe {
            std::arch::x86_64::_rdtsc() as i64
        }
        
        #[cfg(not(target_arch = "x86_64"))]
        {
            use std::time::SystemTime;
            SystemTime::now()
                .duration_since(SystemTime::UNIX_EPOCH)
                .unwrap()
                .as_nanos() as i64
        }
    }
}

// ============================================================================
// Shared Memory Queue (Rust wrapper for C++ shared memory)
// ============================================================================

pub struct SharedMemoryQueue {
    name: String,
    capacity: usize,
    // Will map to C++ SharedMemoryRingBuffer via FFI
}

impl SharedMemoryQueue {
    pub fn new(name: &str, capacity: usize) -> Self {
        Self {
            name: name.to_string(),
            capacity,
        }
    }
    
    // FFI functions to C++
    pub fn write_tick(&self, tick: &MarketTick) -> bool {
        // Calls C++ shared memory write
        unsafe {
            shm_write_tick(self.name.as_ptr(), tick as *const MarketTick)
        }
    }
    
    pub fn read_tick(&self) -> Option<MarketTick> {
        let mut tick = MarketTick::default();
        let success = unsafe {
            shm_read_tick(self.name.as_ptr(), &mut tick as *mut MarketTick)
        };
        
        if success {
            Some(tick)
        } else {
            None
        }
    }
}

// ============================================================================
// Risk Control (Rust implementation with memory safety)
// ============================================================================

pub struct RiskControl {
    max_position: i64,
    current_position: AtomicU64,  // Use u64 and interpret as i64
    kill_switch: AtomicBool,
    total_pnl: AtomicU64,  // Fixed-point representation
}

impl RiskControl {
    pub fn new(max_position: i64) -> Self {
        Self {
            max_position,
            current_position: AtomicU64::new(0),
            kill_switch: AtomicBool::new(false),
            total_pnl: AtomicU64::new(0),
        }
    }
    
    #[inline(always)]
    pub fn check_pre_trade(&self, order: &Order, current_pos: i64) -> bool {
        // Kill switch check
        if self.kill_switch.load(Ordering::Acquire) {
            return false;
        }
        
        // Calculate new position
        let delta = if order.side == 0 {  // BUY
            order.quantity as i64
        } else {
            -(order.quantity as i64)
        };
        
        let new_pos = current_pos + delta;
        
        // Position limit check
        if new_pos.abs() > self.max_position {
            return false;
        }
        
        true
    }
    
    #[inline(always)]
    pub fn trigger_kill_switch(&self) {
        self.kill_switch.store(true, Ordering::Release);
    }
    
    #[inline(always)]
    pub fn is_halted(&self) -> bool {
        self.kill_switch.load(Ordering::Acquire)
    }
}

// ============================================================================
// FFI Declarations (C++ functions callable from Rust)
// ============================================================================

extern "C" {
    fn shm_write_tick(name: *const u8, tick: *const MarketTick) -> bool;
    fn shm_read_tick(name: *const u8, tick: *mut MarketTick) -> bool;
    fn cpp_hawkes_update(engine: *mut std::ffi::c_void, tick: *const MarketTick);
    fn cpp_fpga_predict(engine: *mut std::ffi::c_void, features: *const f64, output: *mut f64);
}

// ============================================================================
// Rust-side Market Making Strategy
// Zero-cost abstractions with compile-time guarantees
// ============================================================================

pub struct MarketMaker {
    risk_aversion: f64,
    volatility: f64,
    tick_size: f64,
}

impl MarketMaker {
    pub fn new(risk_aversion: f64, volatility: f64, tick_size: f64) -> Self {
        Self {
            risk_aversion,
            volatility,
            tick_size,
        }
    }
    
    #[inline(always)]
    pub fn calculate_reservation_price(&self, mid_price: f64, inventory: i64, time_remaining: f64) -> f64 {
        let inventory_penalty = inventory as f64 
            * self.risk_aversion 
            * self.volatility.powi(2) 
            * time_remaining;
        
        mid_price - inventory_penalty
    }
    
    #[inline(always)]
    pub fn calculate_spread(&self, time_remaining: f64, arrival_rate: f64) -> f64 {
        let time_component = self.risk_aversion * self.volatility.powi(2) * time_remaining;
        let arrival_component = (2.0 / self.risk_aversion) * (1.0 + self.risk_aversion / arrival_rate).ln();
        
        let spread = time_component + arrival_component;
        spread.max(self.tick_size * 2.0)
    }
    
    pub fn generate_quotes(&self, tick: &MarketTick, inventory: i64) -> (f64, f64) {
        let reservation_price = self.calculate_reservation_price(
            tick.mid_price, 
            inventory, 
            300.0
        );
        
        let spread = self.calculate_spread(300.0, 10.0);
        let half_spread = spread / 2.0;
        
        // Inventory skew
        let skew_factor = (inventory as f64 / 1000.0).tanh();
        let bid_spread = half_spread * (1.0 - skew_factor);
        let ask_spread = half_spread * (1.0 + skew_factor);
        
        let bid = (reservation_price - bid_spread / self.tick_size).round() * self.tick_size;
        let ask = (reservation_price + ask_spread / self.tick_size).round() * self.tick_size;
        
        (bid, ask)
    }
}

// ============================================================================
// Default implementations
// ============================================================================

impl Default for MarketTick {
    fn default() -> Self {
        Self {
            timestamp_ns: 0,
            bid_price: 0.0,
            ask_price: 0.0,
            mid_price: 0.0,
            bid_size: 0,
            ask_size: 0,
            trade_volume: 0,
            trade_side: 0,
            asset_id: 0,
            depth_levels: 0,
            _padding: [0; 7],
            bid_prices: [0.0; 10],
            ask_prices: [0.0; 10],
            bid_sizes: [0; 10],
            ask_sizes: [0; 10],
        }
    }
}

impl Copy for MarketTick {}
impl Clone for MarketTick {
    fn clone(&self) -> Self {
        *self
    }
}

// ============================================================================
// Benchmarking utilities
// ============================================================================

#[inline(never)]
pub fn benchmark_queue_throughput() {
    const ITERATIONS: usize = 1_000_000;
    let queue: LockFreeSPSC<u64, 16384> = LockFreeSPSC::new();
    
    let start = Instant::now();
    
    for i in 0..ITERATIONS {
        while !queue.push(i as u64) {}
        let _ = queue.pop();
    }
    
    let elapsed = start.elapsed();
    let ns_per_op = elapsed.as_nanos() / (ITERATIONS as u128 * 2);
    
    println!("Rust SPSC Queue: {} ns/op", ns_per_op);
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_queue_basic() {
        let queue: LockFreeSPSC<u64, 16> = LockFreeSPSC::new();
        
        assert!(queue.push(42));
        assert_eq!(queue.pop(), Some(42));
        assert_eq!(queue.pop(), None);
    }
    
    #[test]
    fn test_market_maker() {
        let mm = MarketMaker::new(0.1, 0.2, 0.01);
        let mut tick = MarketTick::default();
        tick.mid_price = 100.0;
        
        let (bid, ask) = mm.generate_quotes(&tick, 0);
        assert!(bid < ask);
        assert!(bid < tick.mid_price);
        assert!(ask > tick.mid_price);
    }
}

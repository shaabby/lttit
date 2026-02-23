## **lttit**

[中文介绍](./docs/中文/README中文.md)

*lttit* is an experimental framework designed to become a **“Tit Cluster Operating System”**—a distributed operating system inspired by the coordinated behavior of bird flocks. It is built around two core capabilities:

1. **Multiple nodes collaborate as a unified system**
2. **Nodes can dynamically modify their logic at runtime**

## **System Components**

lttit consists of several independently portable modules. The foundational modules include:

- **RTOS Kernel** 
   Dual schedulers (RT/BE), synchronization primitives, timers
- **File System (FS)** 
   Lightweight Unix‑like filesystem
- **Memory Management (mg)** 
   Multiple allocators optimized for MCUs
- **Data Structure Library (lib)** 
   Red‑black trees, radix trees, hash tables, linked lists, etc.
- **Network Protocol Stack (TCP/IP)** 
   Simplified TCP/IP implementation
- **Shell & Text Editor** 
   Interactive command‑line shell and lightweight editor

These modules form the basic runtime environment of lttit.
 On top of them sit the two core subsystems: **CSC** and **ccBPF**.

## **Core Capabilities**

### **CSC: Distributed Protocol Stack**

CSC enables multiple MCU nodes to behave like **one machine**, providing:

- **Routing (CCNET)**
- **Reliable Transport (SCP)**
- **Remote Procedure Calls (CCRPC)**

### **ccBPF: Dynamic Programming Engine**

ccBPF is a C‑subset compiler paired with a BPF virtual machine.
 It enables nodes to **modify their behavior at runtime**, supporting dynamic updates to:

- Scheduling policies
- Firewall rules
- Flow‑control algorithms
- Any other node‑level logic

In short:

- **CSC** makes nodes form a unified whole
- **ccBPF** lets that whole **change itself dynamically**

## **lttit Documentation Guide**

This directory contains three categories of documentation:

### **1. User Guides**

- **Quick Start** — How to run lttit quickly
- **System Overview** — High‑level architecture of the system

### **2. Porting & Usage Manuals**

For engineers integrating or porting individual components.

### **3. Design Documents**

For developers who want to understand the internal architecture and design principles of each subsystem.

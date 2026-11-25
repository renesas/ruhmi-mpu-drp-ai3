
# How to use DRP-AI Runtime Wrapper

## List of functions

| Function name                                    | Note                                                        |
| ----------------------------------------- | ----------------------------------------------------------- |
| [GetInputDataType](#getinputdatatype) | Return input data type                                    |
| [GetNumOutput](#getnumoutput)         | Get the number of outputs from the graph.                   |
| [GetOutput](#getoutput)               | Returns inference result                                        |
| [LoadModel](#loadmodel)               | Deploy the DRP-AI model       |
| [ProfileRun](#profilerun)             | Run AI model graph with profile                                  |
| [Run](#run)                           | Run AI model graph                                                   |
| [SetInput](#setinput)                 | Copy data to the input layer                                |

----
## GetInputDataType
 - Return input data type
### Format
```c++
#include "DrpAiRuntimeWrapper.h"

InOutDataType DrpAiRuntimeWrapper::GetInputDataType(int index);
```
### Note
 - **GetInputDataType()** method returns the data type of the input layer specified by the argument index.
### Return   
  - Input data type (`InOutDataType`)
   - FLOAT32 : 32-bit float
   - FLOAT16 : 16-bit float
   - INT32 : 32-bit signed integer
   - INT64 : 64-bit signed integer
   - OTHER : Not supported type 


----
## GetNumOutput
 - Get the number of outputs from the graph.

### Format
```c++
#include "DrpAiRuntimeWrapper.h"

int DrpAiRuntimeWrapper::GetNumOutput(void);
```
### Note
 - **GetNumOutput()** method returns the number of output layers of the inference model deployed to the DrpAiRuntimeWrapper object.

### Return
 - The number of output nodes. A non-zero positive integer. A value of 0 or negative indicates that a problem occurred during the processing of this function.

----
## GetOutput
 - Returns inference result.

### Format
```c++
#include "DrpAiRuntimeWrapper.h"

std::tuple<InOutDataType, void*, int64_t> DrpAiRuntimeWrapper::GetOutput(int index);
```
### Note
 - **GetOutput()**  method returns the inference result as a tuple type object. Also, specify the index of the output layer as an argument.

### Return   
  - Inference results. The data type depends on the AI model graph and is one of the `InOutDataType` values (see [GetInputDataType](#getinputdatatype)). 

----
## LoadModel
 - Deploy the DRP-AI model specified in the DrpAiRuntimeWrapper class object.

### Format
```c++
#include "DrpAiRuntimeWrapper.h"

bool DrpAiRuntimeWrapper::LoadModel(const std::string& model_dir, 
                                      uint64_t start_address);
```
### Note

- Expand the runtime model data into the memory for the DRP-AI device. The directory where inference models and their weights are stored is specified by the argument `model_dir`. The memory address for the DRP-AI device is specified by the argument `start_address`.

### Return

- true : Success.
- false : Failure.

----
## ProfileRun
 - Run AI model graph with profile 

### Format

```c++
#include "DrpAiRuntimeWrapper.h"

void DrpAiRuntimeWrapper::ProfileRun(const std::string& profile_table, 
                                       const std::string& profile_csv);
void DrpAiRuntimeWrapper::ProfileRun(const std::string& profile_table,
                                       const std::string& profile_csv, 
                                       int freq_index); // Works only on the RZ/V2H.
```

### Note
TO BE UPDATED

### Return

- None

----

## Run

- Run AI model graph.

### Format

```c++
#include "DrpAiRuntimeWrapper.h"

void DrpAiRuntimeWrapper::Run(void);
void DrpAiRuntimeWrapper::Run(int freq_index); // Works only on the RZ/V2H.
```

### Note

- **Run()** method performs inference.
  - In the case of RZ/V2H, you can specify the `freq_index` argument to lower the operating frequency of the DRP-AI hardware. For example, setting `freq_index` to **5** will operate the DRP-AI at 315MHz.

### Return

- None

----

## SetInput

- Copy data to the input layer.

### Format
```c++
#include "DrpAiRuntimeWrapper.h"

void DrpAiRuntimeWrapper::SetInput(int input_index, const float* data_ptr);
void DrpAiRuntimeWrapper::SetInput(int input_index, const uint16_t* data_ptr);
```
### Note
 - **SetInput()** method copies the value specified by *data_ptr* to the input layer specified by the argument index. Two overloads are provided for *data_ptr*: `const float*` (32-bit float) and `const uint16_t*` (16-bit floating point).

### Return   
 - None


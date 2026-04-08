# SQL Parser Engine

파일 기반 SQL 처리기를 직접 구현하며 작은 DBMS의 핵심 흐름을 따라가는 프로젝트다.  
이 프로그램은 SQL 파일을 읽고, 문법을 해석하고, 파일 DB 위에서 실행한 뒤, 결과를 화면에 출력한다.

## 1. 프로젝트 한눈에 보기

```mermaid
flowchart LR
    A["SQL 파일"] --> B["Lexer<br/>TokenArray"]
    B --> C["Parser<br/>Statement(AST)"]
    C --> D["Executor"]
    D --> E["Schema / Storage"]
    E --> F["ExecResult"]
    F --> G["결과 출력"]
```

이 프로젝트를 가장 짧게 요약하면 아래와 같다.

```text
읽고 -> 해석하고 -> 실행하고 -> 보여준다
```

## 2. 왜 이 프로젝트를 만들었는가

보통 SQL은 DBMS가 알아서 처리해 주지만, 이번 프로젝트에서는 그 내부 흐름을 직접 구현해 본다.

핵심 목표는 세 가지다.

* SQL 문자열이 어떻게 구조화되는지 이해한다.
* 구조화된 명령이 실제 파일 시스템 위에서 어떻게 실행되는지 이해한다.
* `main -> parser -> executor -> storage` 흐름을 Top-Down으로 설명할 수 있게 만든다.

## 3. 지원 범위

### 지원 SQL

```sql
INSERT INTO users VALUES (1, 'Alice', 20);
INSERT INTO users (id, name, age) VALUES (1, 'Alice', 20);
INSERT INTO users (name, id) VALUES ('Bob', 2);
SELECT * FROM users;
SELECT id, name FROM users;
```

### 제외한 기능

* `CREATE TABLE`
* `UPDATE`
* `DELETE`
* `JOIN`
* `WHERE`
* 여러 SQL 문장 처리
* 인덱스
* 트랜잭션

## 4. Top-Down 관점의 전체 흐름

`study.md`에서 정리한 핵심 학습 포인트를 실제 코드 구조에 맞게 요약하면, 이 프로젝트는 "문자열이 점점 의미 있는 구조로 바뀌는 과정"이다.

```mermaid
flowchart TD
    A["원본 SQL 문자열<br/>SELECT id, name FROM users;"] --> B["Token 목록<br/>[SELECT] [id] [,] [name] [FROM] [users]"]
    B --> C["AST<br/>type=SELECT<br/>table=users<br/>columns=[id,name]"]
    C --> D["실행용 데이터<br/>schema + rows"]
    D --> E["출력 결과<br/>id | name ..."]
```

### 핵심 추상화 3개

* `Token`: 문자열을 의미 단위로 잘라 놓은 결과
* `AST`: SQL 문장의 구조를 프로그램이 이해한 결과
* `Row`: 파일에 저장되거나 파일에서 읽혀 나온 실제 데이터 한 줄

## 5. 모듈 구조

```mermaid
flowchart TB
    MAIN["main.c"] --> CLI["cli.c"]
    MAIN --> UTILS["utils.c"]
    MAIN --> LEXER["lexer.c"]
    MAIN --> PARSER["parser.c"]
    MAIN --> EXECUTOR["executor.c"]
    MAIN --> RESULT["result.c"]
    EXECUTOR --> SCHEMA["schema.c"]
    EXECUTOR --> STORAGE["storage.c"]
```

### 각 모듈의 역할

* `main`: 전체 실행 순서를 조립하는 지휘자
* `cli`: DB 경로와 SQL 파일 경로를 해석
* `lexer`: SQL 문자열을 token으로 분해
* `parser`: token 배열을 AST로 변환
* `executor`: AST를 실제 동작으로 연결
* `schema`: `<table>.schema`를 읽어 테이블 구조 로드
* `storage`: `<table>.data` 파일 읽기/쓰기
* `result`: 실행 결과를 사용자 친화적으로 출력
* `utils`: 파일 읽기, 문자열 처리, 메모리 보조 함수

## 6. 함수 호출 흐름

```mermaid
flowchart TD
    M["main"] --> C1["parse_cli_args"]
    C1 --> U1["read_text_file"]
    U1 --> L1["tokenize_sql"]
    L1 --> P1["parse_statement"]
    P1 --> E1["execute_statement"]
    E1 --> S1["load_table_schema"]
    E1 --> S2["append_row_to_table"]
    E1 --> S3["read_all_rows_from_table"]
    E1 --> R1["print_exec_result"]
```

이 그림이 전체 코드를 읽는 가장 빠른 지도다.

## 7. 저장 구조

테이블은 두 파일로 관리한다.

* `<table>.schema`: 컬럼 메타데이터
* `<table>.data`: 실제 row 데이터

```mermaid
flowchart LR
    subgraph Schema["users.schema"]
        S1["id"]
        S2["name"]
        S3["age"]
    end

    subgraph Data["users.data"]
        D1["1|Alice|20"]
        D2["2|Bob|"]
    end

    Schema -->|"컬럼 순서가 저장 순서를 결정"| Data
```

### 예시

`db/users.schema`

```text
id
name
age
```

`db/users.data`

```text
1|Alice|20
2|Bob|
```

### 저장 규칙

* 한 줄이 한 row다.
* 필드 구분자는 `|`다.
* schema 컬럼 순서가 data 저장 순서다.
* 명시되지 않은 컬럼은 빈 문자열 `""`로 저장한다.
* escape 규칙은 `\\`, `\|`, `\n`을 사용한다.

## 8. INSERT 흐름

```mermaid
sequenceDiagram
    participant User as User
    participant Main as main
    participant Lexer as lexer
    participant Parser as parser
    participant Executor as executor
    participant Schema as schema
    participant Storage as storage
    participant Result as result

    User->>Main: INSERT SQL 파일 실행
    Main->>Lexer: tokenize_sql(sql)
    Lexer-->>Main: TokenArray
    Main->>Parser: parse_statement(tokens)
    Parser-->>Main: Insert AST
    Main->>Executor: execute_statement(stmt)
    Executor->>Schema: load_table_schema("users")
    Executor->>Executor: build_insert_row()
    Executor->>Storage: ensure_table_data_file()
    Executor->>Storage: append_row_to_table()
    Executor-->>Main: ExecResult(INSERT 1)
    Main->>Result: print_exec_result()
```

### 예시 SQL

```sql
INSERT INTO users (name, id) VALUES ('Bob', 2);
```

### 내부적으로 일어나는 일

```mermaid
flowchart LR
    A["입력 컬럼 순서<br/>name, id"] --> B["schema 순서 확인<br/>id, name, age"]
    B --> C["row 재구성<br/>[2, Bob, '']"]
    C --> D["파일 저장<br/>2|Bob|"]
```

즉, `INSERT`에서 가장 중요한 포인트는 입력 컬럼 순서를 schema 순서에 맞게 다시 정렬하는 것이다.

## 9. SELECT 흐름

```mermaid
sequenceDiagram
    participant User as User
    participant Main as main
    participant Parser as parser
    participant Executor as executor
    participant Schema as schema
    participant Storage as storage
    participant Result as result

    User->>Main: SELECT SQL 파일 실행
    Main->>Parser: parse_statement(tokens)
    Parser-->>Main: Select AST
    Main->>Executor: execute_statement(stmt)
    Executor->>Schema: load_table_schema("users")
    Executor->>Storage: read_all_rows_from_table()
    Executor->>Executor: project_rows_for_select()
    Executor-->>Main: QueryResult
    Main->>Result: print_exec_result()
```

### 예시 SQL

```sql
SELECT name, id FROM users;
```

### 내부적으로 일어나는 일

```mermaid
flowchart LR
    A["schema<br/>id, name, age"] --> B["data row<br/>1|Alice|20"]
    B --> C["projection<br/>name, id"]
    C --> D["출력 row<br/>Alice, 1"]
```

즉, `SELECT`에서 가장 중요한 포인트는 필요한 컬럼만 잘라내는 projection이다.

## 10. AST 구조

프로그램은 SQL을 문자열 그대로 다루지 않고 AST로 바꿔 다룬다.

```mermaid
classDiagram
    class Statement {
        +StatementType type
    }

    class InsertStatement {
        +char* table_name
        +char** columns
        +size_t column_count
        +LiteralValue* values
        +size_t value_count
    }

    class SelectStatement {
        +char* table_name
        +int select_all
        +char** columns
        +size_t column_count
    }

    class LiteralValue {
        +ValueType type
        +char* text
    }

    Statement --> InsertStatement
    Statement --> SelectStatement
    InsertStatement --> LiteralValue
```

이 구조 덕분에 Executor는 더 이상 "문자열 SQL"을 해석하지 않고, 이미 정리된 명령 구조를 실행하는 데 집중할 수 있다.

## 11. 디렉터리 구조

```text
sql-parser-engine/
├─ Makefile
├─ README.md
├─ db/
│  ├─ users.schema
│  └─ products.schema
├─ docs/
│  ├─ workOrder.md
│  ├─ workOrder_A.md
│  ├─ workOrder_B.md
│  ├─ workOrder_C.md
│  └─ study.md
├─ include/
│  ├─ ast.h
│  ├─ cli.h
│  ├─ errors.h
│  ├─ executor.h
│  ├─ lexer.h
│  ├─ parser.h
│  ├─ result.h
│  ├─ schema.h
│  ├─ storage.h
│  └─ utils.h
├─ queries/
│  ├─ insert_users.sql
│  ├─ select_users.sql
│  ├─ select_user_names.sql
│  └─ invalid_missing_from.sql
├─ src/
│  ├─ main.c
│  ├─ cli.c
│  ├─ lexer.c
│  ├─ parser.c
│  ├─ executor.c
│  ├─ schema.c
│  ├─ storage.c
│  ├─ result.c
│  └─ utils.c
└─ tests/
   ├─ test_lexer.c
   ├─ test_parser.c
   ├─ test_storage.c
   ├─ test_executor.c
   └─ test_integration.sh
```

## 12. 빌드와 실행

### 빌드

```sh
make
```

프로젝트는 아래 옵션으로 빌드한다.

```sh
-std=c99 -Wall -Wextra -Werror
```

### 실행

```sh
./sql_processor --db ./db --file ./queries/insert_users.sql
./sql_processor -d ./db -f ./queries/select_users.sql
```

### 도움말

```sh
./sql_processor --help
```

## 13. 테스트 전략

```mermaid
flowchart LR
    A["test_lexer.c"] --> U["단위 테스트"]
    B["test_parser.c"] --> U
    C["test_storage.c"] --> U
    D["test_executor.c"] --> U
    U --> E["test_integration.sh"]
    E --> F["실제 바이너리 검증"]
```

### 테스트 실행

```sh
make && make test
```

### 검증 포인트

* Lexer: token 분해가 정확한가
* Parser: AST가 정확히 만들어지는가
* Storage: escape/unescape와 row read/write가 정확한가
* Executor: INSERT/SELECT 규칙이 정확한가
* Integration: 실제 바이너리 실행 결과가 기대값과 같은가

`test_integration.sh`는 `sql_processor` 실행 파일을 사용하므로, 깨끗한 상태에서는 `make`로 먼저 바이너리를 만든 뒤 `make test`를 실행하는 흐름이 가장 안전하다.

## 14. 예제 결과

### INSERT 결과

```text
INSERT 1
```

### SELECT 결과

```text
id | name  | age
----------------
1  | Alice | 20
2  | Bob   |

2 rows selected
```

## 15. 우리가 사용한 협업 방식

이번 프로젝트는 구현만 나누지 않고, 이해의 깊이도 시간대별로 나눠서 가져갔다.

```mermaid
flowchart LR
    A["오전<br/>작업지시서 + 학습자료 작성<br/>Top-Down으로 큰 흐름 이해"] --> B["점심 후<br/>각자 작업지시서 설명<br/>최고안 선정"]
    B --> C["오후 초반<br/>선정된 지시서 기준 분업"]
    C --> D["저녁 전<br/>구현 진행 + low-level 공부"]
    D --> E["저녁 후<br/>발표 준비 + 코드 뜯어보기"]
```

### 이 방식의 장점

* 처음부터 구현에 뛰어들지 않고, 전체 구조를 먼저 공유할 수 있었다.
* 작업지시서와 학습자료를 분리해 “구현”과 “이해”를 동시에 챙길 수 있었다.
* 오후에는 기능 분업, 저녁에는 코드 리딩과 발표 준비로 전환해 학습 효율이 높았다.

### 우리 팀이 실제로 가져간 관점

```mermaid
flowchart TD
    A["Top-Down 이해"] --> B["모듈 경계 확정"]
    B --> C["병렬 구현"]
    C --> D["Low-level 코드 공부"]
    D --> E["발표 자료 정리"]
    E --> F["전체 흐름 재설명"]
```

즉, 오전에는 "전체 그림", 오후에는 "세부 구현", 저녁에는 "설명 가능한 이해"로 전환하는 식으로 협업했다.

## 16. 처음 읽을 때 추천하는 코드 읽기 순서

이 순서는 Top-Down 학습에 가장 잘 맞는다.

```mermaid
flowchart LR
    A["1. main.c"] --> B["2. ast.h"]
    B --> C["3. parser.c"]
    C --> D["4. executor.c"]
    D --> E["5. schema.c / storage.c"]
    E --> F["6. result.c / utils.c"]
```

이 순서가 좋은 이유는 아래와 같다.

* `main.c`: 전체 흐름이 보인다.
* `ast.h`: 프로그램이 SQL을 어떤 구조로 이해하는지 보인다.
* `parser.c`: 문자열이 AST로 바뀌는 지점이 보인다.
* `executor.c`: 가장 중요한 실행 규칙이 보인다.
* `schema.c`, `storage.c`: 파일 기반 DB의 실제 동작이 보인다.
* `result.c`, `utils.c`: 출력과 보조 로직을 마무리로 이해할 수 있다.

## 17. 엣지 케이스

다음 항목을 주요 검증 대상으로 둔다.

* 빈 SQL 파일
* 닫히지 않은 문자열
* 잘못된 토큰
* 존재하지 않는 테이블
* 존재하지 않는 컬럼
* `INSERT` 값 개수 불일치
* schema 파일 비어 있음
* data row column count mismatch
* 부분 컬럼 `INSERT`
* `SELECT *`와 partial `SELECT`

## 18. 한계와 향후 개선점

### 현재 한계

* 단일 SQL 문장만 처리
* `WHERE` 미지원
* 타입 시스템 단순화
* 인덱스와 트랜잭션 미지원
* 복잡한 SQL 문법 미지원

### 향후 개선

* `WHERE column = literal`
* 여러 SQL 문장 처리
* SQL comment 무시
* 출력 정렬 고도화
* `NULL` 지원

## 19. 발표용 한 줄 정리

```text
이 프로젝트는 SQL 문자열을 token으로 자르고,
AST로 구조화하고,
그 구조를 파일 기반 DB 위에서 실행해
결과를 보여주는 작은 SQL 처리기다.
```

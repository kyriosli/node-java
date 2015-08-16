{
  "targets": [
    {
      "target_name": "java",
      "sources": [
        "src/java.cc",
        "src/async.cc",
        "src/bindings.cc",
        "src/bindings_array.cc",
        "src/java_exception.cc",
        "src/java_invoke.cc",
        "src/java_native.cc"
      ],
      "include_dirs" : [
        "<!(node -e \"require('nan')\")"
      ],
      "conditions": [
        ["OS==\"linux\"",
        {
          "include_dirs": [
            "<!(echo $JAVA_HOME)/include",
            "<!(echo $JAVA_HOME)/include/linux"
          ]
        }],
        ["OS==\"mac\"",
        {
          "include_dirs": [
            "<!(/usr/libexec/java_home)/include",
            "<!(/usr/libexec/java_home)/include/darwin"
          ]
        }]
      ]
    }
  ]
}

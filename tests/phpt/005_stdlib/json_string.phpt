--TEST--
JSON and array string replacement
--FILE--
<?php
$value = ['name' => 'pico', 'items' => [1, true, null]];
echo json_encode($value), ':';
echo implode(',', str_replace(['red', 'blue'], ['R', 'B'], ['red', 'blue']));
--EXPECT--
{"name":"pico","items":[1,true,null]}:R,B

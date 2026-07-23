--TEST--
JSON encode and decode vector 005
--FILE--
<?php
$value = ['name' => 'item05', 'number' => 66, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item05","number":66,"enabled":false}:item05:66:0

--TEST--
JSON encode and decode vector 014
--FILE--
<?php
$value = ['name' => 'item14', 'number' => 183, 'enabled' => true];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item14","number":183,"enabled":true}:item14:183:1

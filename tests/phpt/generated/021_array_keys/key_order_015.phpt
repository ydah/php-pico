--TEST--
associative key order vector 015
--FILE--
<?php
$items = ['left' => 25, 4 => 65, 'right' => 105];
unset($items[4]);
$items['tail'] = 106;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:25,105,106:1

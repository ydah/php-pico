--TEST--
associative key order vector 014
--FILE--
<?php
$items = ['left' => 24, 4 => 62, 'right' => 100];
unset($items[4]);
$items['tail'] = 101;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:24,100,101:1

--TEST--
nested array copy-on-write vector 004
--FILE--
<?php
$tree = ['node' => ['value' => 17, 'items' => [18]]];
$copy = $tree;
$copy['node']['value'] += 6;
$copy['node']['items'][] = 25;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
17:23:18:18,25

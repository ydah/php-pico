--TEST--
nested array copy-on-write vector 007
--FILE--
<?php
$tree = ['node' => ['value' => 29, 'items' => [30]]];
$copy = $tree;
$copy['node']['value'] += 2;
$copy['node']['items'][] = 33;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
29:31:30:30,33

--TEST--
nested array copy-on-write vector 033
--FILE--
<?php
$tree = ['node' => ['value' => 133, 'items' => [134]]];
$copy = $tree;
$copy['node']['value'] += 7;
$copy['node']['items'][] = 142;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
133:140:134:134,142
